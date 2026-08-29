#!/usr/bin/env python3
"""Draw every protocol's QASM circuits with Qiskit.

Each `.qasm` under protocols/ is loaded, its custom gates are expanded, and the
result is drawn into a `layout/` folder beside it:

    protocols/CR17_[[5,1,3]]/XZZXI_f.qasm
    protocols/CR17_[[5,1,3]]/layout/XZZXI_f.png

Custom gates are expanded because a layout is exactly what they hide. The
protocols wrap their couplings in `meas_x_component` and friends, and Qiskit
draws an unexpanded circuit as an opaque box -- correct, but it shows the
protocol's vocabulary rather than the gates that actually run. Only the custom
definitions are expanded: asking Qiskit to decompose everything would also take
`h` apart into `U(pi/2, 0, pi)`, which is noise here.

Needs `qiskit_qasm3_import`, which Qiskit's OpenQASM 3 loader relies on. On a
PEP 668 Python (Homebrew, most Linux distros) install it in a virtualenv rather
than system-wide:

    python3 -m venv --system-site-packages .venv-qiskit
    .venv-qiskit/bin/pip install qiskit_qasm3_import
    .venv-qiskit/bin/python tools/draw_layouts.py
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def load_qiskit():
    """Import Qiskit, or explain what is missing and stop."""
    try:
        from qiskit import qasm3
        from qiskit.circuit.library import standard_gates
    except ImportError as exc:
        sys.exit(f"error: qiskit is not importable ({exc}).\n{__doc__.split('Needs')[1]}")

    try:  # the loader defers to this and only complains when actually called
        import qiskit_qasm3_import  # noqa: F401
    except ImportError:
        sys.exit(
            "error: qiskit_qasm3_import is missing; Qiskit's OpenQASM 3 loader needs it.\n"
            "    python3 -m venv --system-site-packages .venv-qiskit\n"
            "    .venv-qiskit/bin/pip install qiskit_qasm3_import\n"
            "    .venv-qiskit/bin/python tools/draw_layouts.py"
        )

    return qasm3, set(standard_gates.get_standard_gate_name_mapping())


def shown(path: Path) -> Path:
    """Repo-relative when it can be, absolute otherwise."""
    try:
        return path.relative_to(REPO)
    except ValueError:
        return path


# Instructions that are not gates and have nothing to expand.
NOT_A_GATE = {"measure", "reset", "barrier", "delay", "initialize", "store"}


def expand_custom_gates(circuit, standard: set[str]):
    """Expand the circuit's own gate definitions, leaving the standard ones alone.

    Repeats until nothing custom is left, since a definition may itself call
    another one.
    """
    for _ in range(8):  # a definition nested eight deep is a bug, not a protocol
        custom = sorted({i.operation.name for i in circuit.data} - standard - NOT_A_GATE)
        if not custom:
            return circuit
        circuit = circuit.decompose(gates_to_decompose=custom)
    raise RuntimeError(f"gate definitions nested too deeply: {custom}")


DECLARATION = re.compile(
    r"^\s*(qubit|bit)\s*(?:\[\s*(\d+)\s*\])?\s+([A-Za-z_][A-Za-z0-9_]*)\s*;", re.M)


def declared_registers(source: Path) -> tuple[list[tuple[str, int]], list[tuple[str, int]]]:
    """The (name, width) of each qubit and bit register, in declaration order."""
    text = re.sub(r"//.*", "", source.read_text(encoding="utf-8"))
    quantum, classical = [], []
    for kind, width, name in DECLARATION.findall(text):
        (quantum if kind == "qubit" else classical).append((name, int(width or 1)))
    return quantum, classical


def relabel(circuit, source: Path):
    """Put the circuit back on registers named as the QASM declared them.

    Qiskit's importer turns a scalar `qubit syn;` into a loose bit, which the
    drawing then labels with a bare index -- so a flag ancilla and a syndrome
    ancilla come out indistinguishable, which is most of what one wants to see
    in a layout. Rebuilding on named registers restores the protocol's own
    vocabulary. Bits are allocated in declaration order, so the index mapping is
    the identity; if the widths ever fail to add up, the original is returned
    untouched rather than risking a scrambled diagram.
    """
    from qiskit import ClassicalRegister, QuantumCircuit, QuantumRegister

    quantum, classical = declared_registers(source)
    if sum(w for _, w in quantum) != circuit.num_qubits:
        return circuit
    if sum(w for _, w in classical) != circuit.num_clbits:
        return circuit

    qregs = [QuantumRegister(w, n) for n, w in quantum]
    cregs = [ClassicalRegister(w, n) for n, w in classical]
    rebuilt = QuantumCircuit(*qregs, *cregs, name=source.stem)

    qubits = [q for reg in qregs for q in reg]
    clbits = [c for reg in cregs for c in reg]
    for item in circuit.data:
        rebuilt.append(
            item.operation,
            [qubits[circuit.find_bit(b).index] for b in item.qubits],
            [clbits[circuit.find_bit(b).index] for b in item.clbits],
        )
    return rebuilt


def draw(circuit, destination: Path, fmt: str, fold: int, scale: float) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if fmt == "text":
        destination.write_text(
            str(circuit.draw(output="text", fold=fold)) + "\n", encoding="utf-8")
        return

    figure = circuit.draw(output="mpl", fold=fold, scale=scale, style="clifford")
    figure.savefig(destination, dpi=150, bbox_inches="tight")
    # Matplotlib keeps every figure alive until closed, and these runs draw
    # dozens of them.
    import matplotlib.pyplot as plt

    plt.close(figure)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "paths", nargs="*", type=Path,
        help="QASM files or folders to search (default: protocols/)")
    parser.add_argument(
        "--format", choices=("png", "text", "both"), default="png",
        help="png needs matplotlib; text is a plain-text diagram (default: png)")
    parser.add_argument(
        "--fold", type=int, default=-1,
        help="wrap the diagram after this many columns; -1 keeps the circuit on "
             "one uninterrupted row (default: -1)")
    parser.add_argument(
        "--scale", type=float, default=1.0, help="png scale factor (default: 1.0)")
    parser.add_argument(
        "--keep-custom-gates", action="store_true",
        help="draw the custom gates as boxes instead of expanding them")
    parser.add_argument(
        "--raw-bit-names", action="store_true",
        help="label wires the way Qiskit's importer does, instead of using the "
             "register names the QASM declared")
    args = parser.parse_args()

    qasm3, standard = load_qiskit()

    roots = [p.resolve() for p in args.paths] or [REPO / "protocols"]
    sources: list[Path] = []
    for root in roots:
        if root.is_dir():
            sources += sorted(p for p in root.rglob("*.qasm") if p.parent.name != "layout")
        elif root.suffix == ".qasm":
            sources.append(root)
        else:
            print(f"  skip (not a .qasm or folder): {root}")
    if not sources:
        sys.exit("error: no .qasm files found")

    formats = ("png", "text") if args.format == "both" else (args.format,)
    drawn = failed = 0
    current_folder = None

    for source in sources:
        if source.parent != current_folder:
            current_folder = source.parent
            print(f"\n{shown(current_folder)}")
        try:
            circuit = qasm3.load(str(source))
            if not args.keep_custom_gates:
                circuit = expand_custom_gates(circuit, standard)
            if not args.raw_bit_names:
                circuit = relabel(circuit, source)
            for fmt in formats:
                out = source.parent / "layout" / f"{source.stem}.{'png' if fmt == 'png' else 'txt'}"
                draw(circuit, out, fmt, args.fold, args.scale)
            print(f"  ok   {source.name:34s} {circuit.num_qubits:2d} qubits, "
                  f"{len(circuit.data):4d} ops")
            drawn += 1
        except Exception as exc:  # one bad circuit must not stop the rest
            print(f"  FAIL {source.name:34s} {type(exc).__name__}: {exc}")
            failed += 1

    print(f"\n{drawn} drawn, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
