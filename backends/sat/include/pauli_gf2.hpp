#pragma once

#include "ftec/qasm.hpp"

#include <cstddef>
#include <map>
#include <set>
#include <tuple>
#include <vector>

// Symbolic Pauli fault propagation over ftec::QasmProgram, tracking the error
// *relative to the ideal circuit* (identity until a fault occurs), following
// the fault model of "Formal Verification of Flagged Fault-Tolerant Error
// Correction Protocols": faults are introduced only at two-qubit gates
// (cx/cy/cz); every other instruction transforms whatever error already
// exists but never originates a new one.
//
// Because this tracks *deviation from ideal* rather than an absolute Pauli
// frame, single-qubit x/y/z gates are true no-ops here: conjugating any
// Pauli by X, Y or Z changes at most its sign, never its (x,z) support, and
// sign is discarded throughout (Sec. 2.1 of the paper). h/s/sdg do act, since
// they are genuine basis changes.
namespace sat {

// A wire: one qubit or classical bit of a named register.
struct Wire {
    std::string reg;
    std::size_t index = 0;

    friend bool operator==(const Wire& a, const Wire& b) {
        return a.reg == b.reg && a.index == b.index;
    }
    friend bool operator<(const Wire& a, const Wire& b) {
        return std::tie(a.reg, a.index) < std::tie(b.reg, b.index);
    }
};

inline Wire to_wire(const ftec::QubitRef& q) { return {q.reg, q.index}; }
inline Wire to_wire(const ftec::BitRef& b) { return {b.reg, b.index}; }

// One of the four boolean sub-variables at a two-qubit gate location: the
// X/Z component of the injected fault on the first/second operand.
enum class FaultPart { XFirst, ZFirst, XSecond, ZSecond };

struct FaultVar {
    // `tag` disambiguates fault variables introduced by different backend
    // step() calls (each call uses a fresh tag), so chaining propagation
    // across circuits -- or building two independent copies for a same-
    // record collision check -- never lets two distinct faults collide on
    // one SAT variable. `location` is local to one propagate() call.
    std::size_t tag = 0;
    std::size_t location = 0;
    FaultPart part = FaultPart::XFirst;

    friend bool operator==(const FaultVar& a, const FaultVar& b) {
        return std::tie(a.tag, a.location, a.part) == std::tie(b.tag, b.location, b.part);
    }
    friend bool operator<(const FaultVar& a, const FaultVar& b) {
        return std::tie(a.tag, a.location, a.part) < std::tie(b.tag, b.location, b.part);
    }
};

// A symbolic Pauli component: the GF(2) sum (XOR) of a set of fault
// variables. A variable present twice cancels, so this set is always kept
// canonical (each variable appears at most once).
using XorTerm = std::set<FaultVar>;

XorTerm xor_combine(const XorTerm& a, const XorTerm& b);

struct SymbolicPauli {
    XorTerm x;
    XorTerm z;

    friend bool operator<(const SymbolicPauli& a, const SymbolicPauli& b) {
        return std::tie(a.x, a.z) < std::tie(b.x, b.z);
    }
};

struct FaultLocation {
    std::size_t instruction_line = 0;
    Wire first;
    Wire second;
};

// A recorded classical outcome: for a Z-basis `measure` (the only kind that
// appears literally in QASM; an X-basis measurement is written as an
// explicit H immediately before a Z-basis measure), the outcome is
// determined by the X-component of the measured qubit's accumulated error.
struct RecordedOutcome {
    Wire creg;
    XorTerm formula;
};

struct PropagationResult {
    std::vector<FaultLocation> fault_locations;
    std::map<Wire, SymbolicPauli> final_state;
    std::vector<RecordedOutcome> outcomes; // in program order
};

// `initial_state` seeds wires (typically the data register) with whatever
// symbolic error they already carry when this circuit starts -- used to
// chain propagation across the sequence of circuits a backend runs along one
// DAG path, since the physical data qubits persist across rounds while
// ancilla registers are always freshly reset. `tag` marks every fault
// variable this call introduces.
PropagationResult propagate(const ftec::QasmProgram& program,
                             const std::map<Wire, SymbolicPauli>& initial_state,
                             std::size_t tag);

} // namespace sat
