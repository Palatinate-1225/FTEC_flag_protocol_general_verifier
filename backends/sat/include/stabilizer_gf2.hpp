#pragma once

#include "fpdl/parser.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

// Binary Pauli algebra over fpdl::CodeSpec's string generators ("XZZXI"),
// mirroring Sec. 2.1-2.2 of "Formal Verification of Flagged Fault-Tolerant
// Error Correction Protocols": global phase is discarded, so a Pauli string
// is just its (x,z) support per qubit.
namespace sat {

struct PauliBit {
    bool x = false;
    bool z = false;
    friend bool operator==(const PauliBit&, const PauliBit&) = default;
};

using PauliString = std::vector<PauliBit>;

class StabilizerError : public std::runtime_error {
public:
    explicit StabilizerError(std::string message) : std::runtime_error(std::move(message)) {}
};

PauliString parse_pauli_string(const std::string& letters);

bool symplectic_product(const PauliString& a, const PauliString& b); // false=commute, true=anticommute
PauliString multiply(const PauliString& a, const PauliString& b);    // bitwise XOR

struct StabilizerCode {
    std::size_t n = 0;
    std::size_t k = 0;
    std::vector<PauliString> generators;        // n-k independent, mutually commuting.
    std::vector<PauliString> logical_operators;  // 2k operators: X1,Z1,...,Xk,Zk.
};

// Parses code.generators and extends them to a full symplectic basis (the
// logical operators), by the same Gram-Schmidt construction used on
// explore/sat-path-verification's stabilizer_code.cpp, adapted to consume
// fpdl::CodeSpec's string Paulis instead of parsing `code:` a second time.
StabilizerCode build_stabilizer_code(const fpdl::CodeSpec& code);

bool in_normalizer(const PauliString& q, const StabilizerCode& code);
bool has_nontrivial_logical_action(const PauliString& q, const StabilizerCode& code);

} // namespace sat
