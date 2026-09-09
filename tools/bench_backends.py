#!/usr/bin/env python3
"""Run every protocol through each backend and tabulate what it cost.

The point of having two decision-diagram backends is that they answer the same
question, so the interesting number is the difference in price and the only
result that matters more is a disagreement. This script reports both: a table
of times and peak memory, and a loud complaint if two backends do not return
the same verdict for a protocol.

    tools/bench_backends.py --build-dir build
    tools/bench_backends.py --backends dd,spbdd,spbdd-fixed --repeat 3
    tools/bench_backends.py --protocol LL25 --markdown

Timing is best-of-`repeat`, which is the right summary for a deterministic
program: the minimum is the run least disturbed by everything else on the
machine. Peak memory is taken from the same run.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Everything the verifier prints that is worth keeping. Anything absent stays
# None rather than raising: a run that timed out or died still belongs in the
# table, with the reason in its own column.
PATTERNS = {
    "code": re.compile(r"^code\s*:\s*(\[\[.*?\]\]), tau = (\d+)", re.M),
    "bmc_bound": re.compile(r"^BMC bound\s*:\s*(\d+)", re.M),
    "paths_reached": re.compile(r"^paths reached\s*:\s*(\d+) of (\d+)", re.M),
    "records_reached": re.compile(r"^records reached\s*:\s*(\d+)", re.M),
    "circuits_run": re.compile(r"^circuits run\s*:\s*(\d+)", re.M),
    "expansion": re.compile(r"^expansion\s*:\s*([\d.]+) s", re.M),
    "traversal": re.compile(r"^traversal\s*:\s*([\d.]+) s", re.M),
    "total": re.compile(r"^total runtime\s*:\s*([\d.]+) s", re.M),
    "peak_memory": re.compile(r"^peak memory\s*:\s*([\d.]+) (KiB|MiB|GiB)", re.M),
    "failures": re.compile(r"^(\d+) unprotected path\(s\); first failure at t = (\d+)", re.M),
    # spbdd only: how often reordering actually fired and what it cost. This
    # is the number that separates "which method" from "how often", which the
    # method name on its own does not.
    "reordering": re.compile(r"^reordering\s*:.*?, (\d+) run\(s\), (\d+) ms", re.M),
}

UNIT_BYTES = {"KiB": 1024, "MiB": 1024**2, "GiB": 1024**3}


@dataclass
class Run:
    protocol: str
    backend: str
    ok: bool = False
    note: str = ""
    code: str | None = None
    tau: int | None = None
    paths_reached: int | None = None
    paths_total: int | None = None
    records_reached: int | None = None
    circuits_run: int | None = None
    expansion: float | None = None
    traversal: float | None = None
    total: float | None = None
    peak_bytes: int | None = None
    clean: bool | None = None
    failure_count: int = 0
    min_fault_count: int | None = None
    reorder_runs: int | None = None
    reorder_ms: int | None = None
    wall_times: list[float] = field(default_factory=list)

    # What the two backends have to agree on. Timings and memory are excluded
    # on purpose -- differing there is the whole point.
    def verdict(self) -> tuple:
        return (
            self.clean,
            self.failure_count,
            self.min_fault_count,
            self.paths_reached,
            self.records_reached,
            self.circuits_run,
        )


def parse_output(text: str, run: Run) -> None:
    if match := PATTERNS["code"].search(text):
        run.code, run.tau = match.group(1), int(match.group(2))
    if match := PATTERNS["paths_reached"].search(text):
        run.paths_reached, run.paths_total = int(match.group(1)), int(match.group(2))
    for key in ("records_reached", "circuits_run"):
        if match := PATTERNS[key].search(text):
            setattr(run, key, int(match.group(1)))
    for key in ("expansion", "traversal", "total"):
        if match := PATTERNS[key].search(text):
            setattr(run, key, float(match.group(1)))
    if match := PATTERNS["peak_memory"].search(text):
        run.peak_bytes = int(float(match.group(1)) * UNIT_BYTES[match.group(2)])
    if match := PATTERNS["reordering"].search(text):
        run.reorder_runs, run.reorder_ms = int(match.group(1)), int(match.group(2))
    if match := PATTERNS["failures"].search(text):
        run.clean = False
        run.failure_count = int(match.group(1))
        run.min_fault_count = int(match.group(2))
    elif "no unprotected path found" in text:
        run.clean = True


def measure(binary: Path, protocol: Path, backend: str, repeat: int, timeout: float,
            extra: list[str]) -> Run:
    """Run one (protocol, backend) pair `repeat` times and keep the best."""
    run = Run(protocol=protocol.stem, backend=backend)
    best: Run | None = None

    for _ in range(repeat):
        command = [str(binary), str(protocol), f"--backend={backend}", *extra]
        started = time.perf_counter()
        try:
            done = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            run.note = f"timeout after {timeout:g}s"
            return run
        elapsed = time.perf_counter() - started

        if done.returncode != 0:
            run.note = (done.stderr.strip().splitlines() or ["exit " + str(done.returncode)])[-1]
            return run

        attempt = Run(protocol=protocol.stem, backend=backend, ok=True)
        parse_output(done.stdout, attempt)
        attempt.wall_times = [elapsed]
        if best is None or (attempt.traversal or elapsed) < (best.traversal or float("inf")):
            best, keep = attempt, best
            if keep is not None:
                best.wall_times = keep.wall_times + attempt.wall_times
        else:
            best.wall_times.append(elapsed)

    return best if best is not None else run


def human_seconds(value: float | None) -> str:
    if value is None:
        return "-"
    return f"{value:.3f}" if value < 10 else f"{value:.1f}"


def human_bytes(value: int | None) -> str:
    if value is None:
        return "-"
    for unit, scale in (("GiB", 1024**3), ("MiB", 1024**2), ("KiB", 1024)):
        if value >= scale:
            return f"{value / scale:.1f} {unit}"
    return f"{value} B"


def verdict_text(run: Run) -> str:
    if not run.ok:
        # A backend can fail with a paragraph, and one paragraph in one cell
        # makes the whole table unreadable. The full text is in --json.
        note = run.note or "failed"
        return note if len(note) <= 60 else note[:57] + "..."
    if run.clean is None:
        return "?"
    if run.clean:
        return "clean"
    return f"{run.failure_count} bad @t={run.min_fault_count}"


def render(rows: list[list[str]], headers: list[str], markdown: bool) -> str:
    widths = [max(len(str(cell)) for cell in column) for column in zip(headers, *rows)] \
        if rows else [len(h) for h in headers]

    def line(cells: list[str], sep: str) -> str:
        return sep + sep.join(f" {cell:<{widths[i]}} " for i, cell in enumerate(cells)) + sep

    if markdown:
        out = [line(headers, "|"), "|" + "|".join("-" * (w + 2) for w in widths) + "|"]
        out += [line(row, "|") for row in rows]
        return "\n".join(out)

    out = ["  ".join(f"{cell:<{widths[i]}}" for i, cell in enumerate(headers))]
    out.append("  ".join("-" * w for w in widths))
    out += ["  ".join(f"{cell:<{widths[i]}}" for i, cell in enumerate(row)) for row in rows]
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=REPO_ROOT / "build",
                        help="where ftec-verify was built (default: build/)")
    parser.add_argument("--binary", type=Path, default=None,
                        help="the ftec-verify to run, overriding --build-dir")
    parser.add_argument("--protocols", type=Path, default=REPO_ROOT / "protocols",
                        help="directory of protocol directories (default: protocols/)")
    parser.add_argument("--protocol", action="append", default=[],
                        help="only protocols whose name contains this; repeatable")
    parser.add_argument("--backends", default="dd,spbdd",
                        help="comma-separated, in the order to run them (default: dd,spbdd)")
    parser.add_argument("--repeat", type=int, default=1, help="runs per pair; best is kept")
    parser.add_argument("--timeout", type=float, default=900.0, help="seconds per run")
    parser.add_argument("--first", action="store_true",
                        help="pass --first to the verifier (stop at the first bad path)")
    parser.add_argument("--markdown", action="store_true", help="emit a Markdown table")
    parser.add_argument("--json", type=Path, default=None, help="also write the raw numbers here")
    args = parser.parse_args()

    binary = args.binary or (args.build_dir / "ftec-verify")
    if not binary.exists():
        found = shutil.which("ftec-verify")
        if found is None:
            print(f"error: {binary} not found; build it first or pass --binary", file=sys.stderr)
            return 2
        binary = Path(found)

    backends = [name.strip() for name in args.backends.split(",") if name.strip()]
    protocols = sorted(args.protocols.glob("*/*.fpdl"))
    if args.protocol:
        protocols = [p for p in protocols
                     if any(needle in str(p) for needle in args.protocol)]
    if not protocols:
        print("error: no protocols matched", file=sys.stderr)
        return 2

    extra = ["--first"] if args.first else []
    results: dict[str, dict[str, Run]] = {}

    for protocol in protocols:
        name = protocol.stem
        results[name] = {}
        for backend in backends:
            print(f"  running {name} on {backend} ...", file=sys.stderr, flush=True)
            results[name][backend] = measure(binary, protocol, backend, args.repeat,
                                             args.timeout, extra)

    # --- disagreements -----------------------------------------------------
    # Checked before anything is printed, because a backend that answers a
    # different question has no business appearing in a table of times.
    disagreements: list[str] = []
    for name, per_backend in results.items():
        verdicts = {b: r.verdict() for b, r in per_backend.items() if r.ok}
        if len(set(verdicts.values())) > 1:
            disagreements.append(f"{name}: " + "; ".join(f"{b}={v}" for b, v in verdicts.items()))

    # --- the table ---------------------------------------------------------
    # Two backends fit side by side; a sweep over reordering methods does not,
    # so past that it goes long, one row per (protocol, backend). The ratio is
    # always against the first backend named, which makes it the baseline.
    baseline = backends[0]
    rows: list[list[str]] = []

    def ratio_text(run: Run, against: Run) -> str:
        if not (run.traversal and against.traversal):
            return "-"
        return "1.00x" if run.backend == against.backend else f"{against.traversal / run.traversal:.2f}x"

    if len(backends) <= 2:
        headers = ["protocol", "code", "verdict"]
        for backend in backends:
            headers += [f"{backend} time", f"{backend} peak"]
        if len(backends) == 2:
            headers.append(f"{backends[1]} vs {baseline}")

        for name, per_backend in results.items():
            runs = [per_backend[b] for b in backends]
            done = [r for r in runs if r.ok]
            row = [name,
                   next((r.code for r in done if r.code), "-"),
                   verdict_text(done[0]) if done else verdict_text(runs[0])]
            for run in runs:
                row += [human_seconds(run.traversal) if run.ok else "-",
                        human_bytes(run.peak_bytes) if run.ok else "-"]
            if len(backends) == 2:
                row.append(ratio_text(runs[1], runs[0]))
            rows.append(row)
    else:
        headers = ["protocol", "backend", "verdict", "traversal", "peak", f"vs {baseline}",
                   "reorders", "reorder time"]
        for name, per_backend in results.items():
            for backend in backends:
                run = per_backend[backend]
                rows.append([name if backend == baseline else "",
                             backend,
                             verdict_text(run),
                             human_seconds(run.traversal) if run.ok else "-",
                             human_bytes(run.peak_bytes) if run.ok else "-",
                             ratio_text(run, per_backend[baseline]),
                             "-" if run.reorder_runs is None else str(run.reorder_runs),
                             "-" if run.reorder_ms is None else f"{run.reorder_ms / 1000:.1f} s"])

    print()
    print(render(rows, headers, args.markdown))
    print()
    print(f"times are traversal seconds, best of {args.repeat}; "
          f"peak is the whole process, so it includes expansion")

    # --- the summary -------------------------------------------------------
    # Ratios are only ever taken within one protocol, never across: the
    # absolute seconds move with whatever else the machine is doing, and the
    # baseline moves with them.
    for backend in backends[1:]:
        ratios = [results[n][baseline].traversal / results[n][backend].traversal
                  for n in results
                  if results[n][baseline].traversal and results[n][backend].traversal]
        if not ratios:
            continue
        print(f"{backend:<24} vs {baseline}: median {statistics.median(ratios):.2f}x, "
              f"range {min(ratios):.2f}x to {max(ratios):.2f}x "
              f"(above 1 means {backend} is faster)")

    if disagreements:
        print()
        print("DISAGREEMENT -- the backends answer the same question, so this is a bug:")
        for line_text in disagreements:
            print(f"  {line_text}")

    if args.json:
        args.json.write_text(json.dumps(
            {name: {b: vars(run) for b, run in per.items()} for name, per in results.items()},
            indent=2, default=str))
        print(f"\nraw numbers written to {args.json}")

    return 1 if disagreements else 0


if __name__ == "__main__":
    sys.exit(main())
