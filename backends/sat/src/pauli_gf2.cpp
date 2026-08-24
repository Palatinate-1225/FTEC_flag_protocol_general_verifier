#include "pauli_gf2.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace sat {

XorTerm xor_combine(const XorTerm& a, const XorTerm& b) {
    XorTerm result;
    std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(),
                                   std::inserter(result, result.begin()));
    return result;
}

namespace {

XorTerm single(const FaultVar& v) { return XorTerm{v}; }

SymbolicPauli& wire_state(std::map<Wire, SymbolicPauli>& state, const Wire& wire) {
    return state[wire]; // default-constructs (empty, i.e. "no error") on first use.
}

// Injects a fault at `loc` (a fresh two-qubit-gate location) onto the
// post-gate states of its two operands.
void inject_fault(SymbolicPauli& c, SymbolicPauli& t, std::size_t tag, std::size_t loc) {
    c.x = xor_combine(c.x, single({tag, loc, FaultPart::XFirst}));
    c.z = xor_combine(c.z, single({tag, loc, FaultPart::ZFirst}));
    t.x = xor_combine(t.x, single({tag, loc, FaultPart::XSecond}));
    t.z = xor_combine(t.z, single({tag, loc, FaultPart::ZSecond}));
}

} // namespace

PropagationResult propagate(const ftec::QasmProgram& program,
                             const std::map<Wire, SymbolicPauli>& initial_state, std::size_t tag) {
    PropagationResult result;
    std::map<Wire, SymbolicPauli> state = initial_state;

    for (std::size_t i = 0; i < program.instructions.size(); ++i) {
        const ftec::QasmInstruction& instr = program.instructions[i];
        switch (instr.kind) {
            case ftec::QasmInstruction::Kind::Reset: {
                wire_state(state, to_wire(instr.qubits.at(0))) = SymbolicPauli{};
                break;
            }
            case ftec::QasmInstruction::Kind::Barrier:
                break;
            case ftec::QasmInstruction::Kind::Measure: {
                const SymbolicPauli& s = wire_state(state, to_wire(instr.qubits.at(0)));
                result.outcomes.push_back({to_wire(instr.target), s.x});
                break;
            }
            case ftec::QasmInstruction::Kind::Gate: {
                const std::string& gate = instr.gate;
                if (gate == "i" || gate == "id" || gate == "x" || gate == "y" || gate == "z") {
                    // No-op here: this tracks error *relative to the ideal
                    // circuit*, and conjugating any Pauli by X/Y/Z changes at
                    // most its sign (discarded throughout), never its (x,z)
                    // support.
                    break;
                }
                if (gate == "h") {
                    SymbolicPauli& s = wire_state(state, to_wire(instr.qubits.at(0)));
                    std::swap(s.x, s.z);
                    break;
                }
                if (gate == "s" || gate == "sdg") {
                    // Support-only representation: S maps X->Y, Z->Z, Y->X,
                    // i.e. (x,z) -> (x, x xor z). S^2 = Z acts as the
                    // identity on support, so S and Sdg coincide here; only
                    // the discarded phase differs.
                    SymbolicPauli& s = wire_state(state, to_wire(instr.qubits.at(0)));
                    s.z = xor_combine(s.x, s.z);
                    break;
                }
                if (gate == "cx") {
                    const Wire control = to_wire(instr.qubits.at(0));
                    const Wire target = to_wire(instr.qubits.at(1));
                    const SymbolicPauli orig_c = wire_state(state, control);
                    const SymbolicPauli orig_t = wire_state(state, target);

                    SymbolicPauli new_c{orig_c.x, xor_combine(orig_c.z, orig_t.z)};
                    SymbolicPauli new_t{xor_combine(orig_t.x, orig_c.x), orig_t.z};

                    const std::size_t loc = result.fault_locations.size();
                    result.fault_locations.push_back({i, control, target});
                    inject_fault(new_c, new_t, tag, loc);
                    state[control] = std::move(new_c);
                    state[target] = std::move(new_t);
                    break;
                }
                if (gate == "cz") {
                    const Wire q1 = to_wire(instr.qubits.at(0));
                    const Wire q2 = to_wire(instr.qubits.at(1));
                    const SymbolicPauli orig_1 = wire_state(state, q1);
                    const SymbolicPauli orig_2 = wire_state(state, q2);

                    SymbolicPauli new_1{orig_1.x, xor_combine(orig_1.z, orig_2.x)};
                    SymbolicPauli new_2{orig_2.x, xor_combine(orig_2.z, orig_1.x)};

                    const std::size_t loc = result.fault_locations.size();
                    result.fault_locations.push_back({i, q1, q2});
                    inject_fault(new_1, new_2, tag, loc);
                    state[q1] = std::move(new_1);
                    state[q2] = std::move(new_2);
                    break;
                }
                if (gate == "cy") {
                    // Derived by conjugating CX's table by S on the target
                    // (CY = (I(x)S) CX (I(x)Sdg), up to phase):
                    //   new_c.x = xc
                    //   new_c.z = zc xor zt
                    //   new_t.x = xc xor xt
                    //   new_t.z = xc xor xt xor zt
                    const Wire control = to_wire(instr.qubits.at(0));
                    const Wire target = to_wire(instr.qubits.at(1));
                    const SymbolicPauli orig_c = wire_state(state, control);
                    const SymbolicPauli orig_t = wire_state(state, target);

                    SymbolicPauli new_c{orig_c.x, xor_combine(orig_c.z, orig_t.z)};
                    const XorTerm xc_xor_xt = xor_combine(orig_c.x, orig_t.x);
                    SymbolicPauli new_t{xc_xor_xt, xor_combine(xc_xor_xt, orig_t.z)};

                    const std::size_t loc = result.fault_locations.size();
                    result.fault_locations.push_back({i, control, target});
                    inject_fault(new_c, new_t, tag, loc);
                    state[control] = std::move(new_c);
                    state[target] = std::move(new_t);
                    break;
                }
                throw std::runtime_error("sat backend: unsupported gate '" + gate + "'");
            }
        }
    }

    result.final_state = std::move(state);
    return result;
}

} // namespace sat
