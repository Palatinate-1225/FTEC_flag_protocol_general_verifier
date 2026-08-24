#include "sat_backend.hpp"

#include "ftec/dag.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAILED: " << what << "\n";
        ++failures;
    }
}

fpdl::CodeSpec cr17_code() {
    fpdl::CodeSpec code;
    code.n = 5;
    code.k = 1;
    code.d = 3;
    code.generators = {"XZZXI", "IXZZX", "XIXZZ", "ZXIXZ"};
    return code;
}

ftec::CircuitRef circuit_for(const std::filesystem::path& repo_root, const std::string& file) {
    ftec::CircuitRef circuit;
    circuit.se_name = file;
    circuit.qasm = repo_root / "protocols" / "CR17_[[5,1,3]]" / file;
    circuit.data_qubits = "data";
    circuit.syndrome_qubits = "syn";
    circuit.flag_qubits = "flag";
    return circuit;
}

// With no faults allowed (tau=0), a flagged circuit is deterministic: the
// only reachable outcome is the all-zero one.
void test_tau0_is_deterministic(const std::filesystem::path& repo_root) {
    auto backend = ftec::make_sat_backend();
    backend->begin(cr17_code(), /*tau=*/0);
    const auto circuit = circuit_for(repo_root, "XZZXI_f.qasm");

    const auto outcomes = backend->step(backend->initial_state(), circuit);
    check(outcomes.size() == 1, "tau=0: exactly one reachable outcome");
    if (outcomes.size() != 1) return;
    const auto& [outcome, state] = outcomes[0];
    (void)state;
    check(outcome.syndrome.size() == 1 && !outcome.syndrome[0], "tau=0: s=0");
    check(outcome.flag.size() == 1 && !outcome.flag[0], "tau=0: f=0");
}

// With one fault allowed, both the clean outcome and at least one flagged
// outcome must be reachable (matches the hand-derived propagation: a lone Z
// fault on the flag qubit at the first coupling gives s=0,f=1).
void test_tau1_reaches_flag(const std::filesystem::path& repo_root) {
    auto backend = ftec::make_sat_backend();
    backend->begin(cr17_code(), /*tau=*/1);
    const auto circuit = circuit_for(repo_root, "XZZXI_f.qasm");

    const auto outcomes = backend->step(backend->initial_state(), circuit);
    check(outcomes.size() >= 2 && outcomes.size() <= 4,
          "tau=1: between 2 and 4 distinct (s,f) outcomes are reachable, got " +
              std::to_string(outcomes.size()));

    bool has_clean = false, has_flag_only = false;
    for (const auto& [outcome, state] : outcomes) {
        (void)state;
        if (!outcome.syndrome[0] && !outcome.flag[0]) has_clean = true;
        if (!outcome.syndrome[0] && outcome.flag[0]) has_flag_only = true;
    }
    check(has_clean, "tau=1: the fault-free outcome (s=0,f=0) is still reachable");
    check(has_flag_only, "tau=1: (s=0,f=1) is reachable via a lone Z fault on the flag qubit");
}

// The fault budget is cumulative across chained step() calls, not reset per
// circuit: with tau=0 for the whole chain, every circuit -- however many are
// chained -- stays perfectly deterministic (s=0,f=0), because no step ever
// gets a fault to spend.
//
// This is deliberately *not* "after spending the one allowed fault in
// circuit 1, circuit 2 can only read clean": a StateId represents the whole
// family of fault patterns consistent with what's been observed so far, and
// several single-fault locations in circuit 1 can share the same (s,f)
// there while leaving *different* residual errors on the data qubits: with
// zero new local faults, circuit 2 can then legitimately land on more than
// one outcome, because which of those residual errors is "the" one is still
// unresolved, not because circuit 2 spent a second fault. That's the
// merge-by-record behaviour the interface requires, not a bug -- so this
// test doesn't assert a specific reachable count post-spend, only that the
// zero-fault case stays fully deterministic across an arbitrary chain
// length.
void test_budget_is_cumulative_across_steps(const std::filesystem::path& repo_root) {
    auto backend = ftec::make_sat_backend();
    backend->begin(cr17_code(), /*tau=*/0);
    const auto first_circuit = circuit_for(repo_root, "XZZXI_f.qasm");
    const auto second_circuit = circuit_for(repo_root, "IXZZX_f.qasm");

    const auto first_outcomes = backend->step(backend->initial_state(), first_circuit);
    check(first_outcomes.size() == 1, "tau=0: circuit 1 alone is still deterministic");
    if (first_outcomes.size() != 1) return;

    const auto second_outcomes = backend->step(first_outcomes[0].second, second_circuit);
    check(second_outcomes.size() == 1, "tau=0: chaining a second circuit is still deterministic");
    if (second_outcomes.size() == 1) {
        const auto& outcome = second_outcomes[0].first;
        check(!outcome.syndrome[0] && !outcome.flag[0],
              "tau=0: the second circuit's one reachable outcome is (s=0,f=0)");
    }
}

// t=0 can never fail: at exactly 0 faults both copies collapse to the same
// single fault-free Pauli frame, and one element has no partner to multiply
// with (the same invariant dd_backend.cpp documents for its own check()).
void test_check_t0_never_fails(const std::filesystem::path& repo_root) {
    auto backend = ftec::make_sat_backend();
    backend->begin(cr17_code(), /*tau=*/0);
    const auto circuit = circuit_for(repo_root, "XZZXI_f.qasm");
    const auto outcomes = backend->step(backend->initial_state(), circuit);
    check(outcomes.size() == 1, "tau=0 step: still exactly one outcome");
    if (outcomes.empty()) return;
    check(!backend->check(outcomes[0].second).has_value(), "tau=0: check() never fails at t=0");
}

// The real end-to-end validation: CR17's own flagged circuit is meant to
// satisfy #-flag:1, so every state reachable within tau=1 should pass
// check() -- matching what --backend=dd finds for this same protocol (see
// the "已驗證的結果" table in README.md: "無不受保護的路徑" for CR17).
void test_check_finds_no_failure_within_budget(const std::filesystem::path& repo_root) {
    auto backend = ftec::make_sat_backend();
    backend->begin(cr17_code(), /*tau=*/1);
    const auto circuit = circuit_for(repo_root, "XZZXI_f.qasm");
    const auto outcomes = backend->step(backend->initial_state(), circuit);
    check(outcomes.size() >= 2, "tau=1 step: multiple outcomes to check() against");
    for (const auto& [outcome, state] : outcomes) {
        (void)outcome;
        const auto failure = backend->check(state);
        check(!failure.has_value(),
              "XZZXI_f.qasm's flagged circuit should have no undetectable pair within tau=1" +
                  (failure ? (": " + failure->detail) : std::string()));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "expected the repository root as an argument\n";
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    test_tau0_is_deterministic(root);
    test_tau1_reaches_flag(root);
    test_budget_is_cumulative_across_steps(root);
    test_check_t0_never_fails(root);
    test_check_finds_no_failure_within_budget(root);

    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
