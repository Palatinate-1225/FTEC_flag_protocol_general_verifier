#include "pauli_gf2.hpp"

#include "ftec/qasm.hpp"

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

sat::XorTerm term(std::initializer_list<sat::FaultVar> vars) {
    return sat::XorTerm(vars.begin(), vars.end());
}

void test_xor_combine() {
    using sat::FaultPart;
    using sat::FaultVar;
    const FaultVar a{0, 0, FaultPart::XFirst};
    const FaultVar b{0, 0, FaultPart::ZFirst};
    const FaultVar c{0, 1, FaultPart::XFirst};

    check(sat::xor_combine(term({a, b}), term({b, c})) == term({a, c}),
          "xor_combine cancels the shared variable and keeps the rest");
    check(sat::xor_combine(term({a}), term({a})).empty(),
          "xor_combine of a term with itself is empty (GF(2) self-cancellation)");
}

// Validates propagation against XZZXI_f.qasm using the exact same
// hand-derived expectations as the pre-ftec::Backend implementation (see
// tests/pauli_propagation_tests.cpp on explore/sat-path-verification): six
// CX fault locations L0..L5, m = XOR of 7 terms, f = XOR of 5 terms. Running
// through ftec::parse_qasm_file (custom-gate inlining etc.) rather than a
// bespoke parser is the only thing that changed; the circuit and the
// propagation math are identical, so the expected formulas are identical.
void test_xzzxi_circuit(const std::filesystem::path& repo_root) {
    const auto path = repo_root / "protocols" / "CR17_[[5,1,3]]" / "XZZXI_f.qasm";
    const auto program = ftec::parse_qasm_file(path);
    const auto result = sat::propagate(program, {}, /*tag=*/0);

    check(result.fault_locations.size() == 6,
          "XZZXI_f.qasm has exactly six two-qubit-gate fault locations");
    check(result.outcomes.size() == 2, "XZZXI_f.qasm records exactly two outcomes (m and f)");

    using sat::FaultPart;
    using sat::FaultVar;
    const sat::XorTerm* m_formula = nullptr;
    const sat::XorTerm* f_formula = nullptr;
    for (const auto& outcome : result.outcomes) {
        if (outcome.creg.reg == "m") m_formula = &outcome.formula;
        if (outcome.creg.reg == "f") f_formula = &outcome.formula;
    }
    check(m_formula != nullptr, "an outcome is recorded into register 'm'");
    check(f_formula != nullptr, "an outcome is recorded into register 'f'");
    if (m_formula == nullptr || f_formula == nullptr) return;

    check(m_formula->size() == 7, "m is the XOR of exactly 7 fault variables");
    check(f_formula->size() == 5, "f is the XOR of exactly 5 fault variables");

    // The flag qubit is prepared in |+> and measured in the X basis, so only
    // a Z-type fault injected directly on it is caught; an X-type fault at
    // the same location is not.
    check(f_formula->count({0, 1, FaultPart::ZFirst}) == 1,
          "a Z fault on the flag qubit at the first coupling (L1) is caught by the flag");
    check(f_formula->count({0, 1, FaultPart::XFirst}) == 0,
          "an X fault on the flag qubit at the first coupling (L1) is not caught by the flag");
    check(f_formula->count({0, 4, FaultPart::ZFirst}) == 1,
          "a Z fault on the flag qubit at the second coupling (L4) is caught by the flag");

    // syn is always the CX target, so a fault injected directly on it at the
    // very first location reaches the final Z-basis measurement unchanged.
    check(m_formula->count({0, 0, FaultPart::XSecond}) == 1,
          "an X fault on syn at the very first coupling (L0) reaches the syndrome measurement");
    check(m_formula->count({0, 1, FaultPart::XFirst}) == 1,
          "an X fault on the flag qubit at L1 is relayed onto the syndrome bit via the L4 coupling");
}

// Chains two circuits the way a backend's step() must: the second circuit's
// data qubits start from whatever the first circuit's propagation left
// there, not from a clean slate. A fault on data[0] that XZZXI_f.qasm's own
// flag never catches should still be visible in whatever the *next* circuit
// measures about data[0], with a *different* tag disambiguating the two
// circuits' fault variables.
void test_chaining_across_circuits(const std::filesystem::path& repo_root) {
    const auto path = repo_root / "protocols" / "CR17_[[5,1,3]]" / "XZZXI_f.qasm";
    const auto program = ftec::parse_qasm_file(path);

    const auto first = sat::propagate(program, {}, /*tag=*/0);
    std::map<sat::Wire, sat::SymbolicPauli> carried;
    for (const auto& [wire, pauli] : first.final_state) {
        if (wire.reg == "data") carried[wire] = pauli;
    }
    check(!carried.empty(), "the first circuit leaves symbolic error state on the data register");

    const auto second = sat::propagate(program, carried, /*tag=*/1);
    // Every fault variable the second run's outcomes depend on must carry
    // tag 0 (inherited from the first circuit, via data[0..4]) or tag 1
    // (this circuit's own locations) -- never some other tag, and the two
    // tags must not collide with each other's location numbering.
    bool saw_tag0 = false, saw_tag1 = false;
    for (const auto& outcome : second.outcomes) {
        for (const auto& v : outcome.formula) {
            check(v.tag == 0 || v.tag == 1, "second run's outcomes only reference tag 0 or tag 1");
            saw_tag0 |= (v.tag == 0);
            saw_tag1 |= (v.tag == 1);
        }
    }
    check(saw_tag0, "the second circuit's outcomes can still depend on the first circuit's faults "
                     "(carried data-qubit error)");
    check(saw_tag1, "the second circuit's outcomes depend on its own (tag 1) fault locations too");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "expected the repository root as an argument\n";
        return 2;
    }
    test_xor_combine();
    test_xzzxi_circuit(std::filesystem::path(argv[1]));
    test_chaining_across_circuits(std::filesystem::path(argv[1]));

    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
