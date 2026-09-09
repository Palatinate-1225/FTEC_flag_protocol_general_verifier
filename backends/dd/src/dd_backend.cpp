#include "dd_backend.hpp"

#include "ftec/qasm.hpp"
#include "pauli_bdd.hpp"
#include "stabilizer.hpp"

#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ftec {

namespace {

using pbdd::PauliSetBDD;

// Where each register of a circuit lands in the global qubit numbering.
//
// The data qubits are fixed at 0..n-1 and carry the encoded state across the
// whole protocol. Ancillas sit above them and are laid out per circuit, which
// is only sound because every step begins and ends with them at identity: two
// circuits may use the same slot for different purposes, but never at the same
// time.
struct Layout {
    std::map<std::string, int> base;     // register name -> first global qubit
    std::vector<int>           ancillas; // every ancilla qubit, for the resets
    int                        total = 0;
};

class DdBackend : public Backend {
public:
    ~DdBackend() override { release(); }

    void begin(const fpdl::CodeSpec& code, int tau) override {
        release();
        if (code.generators.empty()) {
            throw std::runtime_error("dd backend: the protocol declares no stabilizer generators");
        }
        data_qubits_ = code.n;
        tau_         = tau;

        PauliSetBDD::init(data_qubits_);
        owns_session_ = true;
        allocated_    = data_qubits_;

        // StabilizerCode validates independence and commutation and says so
        // clearly if the generators are not a stabilizer group.
        code_ = std::make_unique<pbdd::StabilizerCode>(code.n, code.generators);

        states_.clear();
        states_.push_back(fresh_state());
    }

    StateId initial_state() override { return 0; }

    std::vector<std::pair<Outcome, StateId>> step(StateId id,
                                                  const CircuitRef& circuit) override {
        ++step_calls_;
        // Two different measurement records often leave the same set of errors
        // behind -- the record is what the decoder saw, not what survived --
        // and stepping depends only on the set. BuDDy hash-conses, so equal
        // sets are literally the same nodes and a key of their roots is exact.
        const CacheKey key{fingerprint(states_[id]), circuit.qasm.string()};
        if (const auto found = step_cache_.find(key); found != step_cache_.end()) {
            ++step_hits_;
            return found->second;
        }

        const QasmProgram& program = program_for(circuit);
        const Layout       layout  = layout_for(program, circuit);
        const Records&     records = records_for(circuit, program);
        grow_to(layout.total);

        // One branch per distinct classical outcome so far. Branches whose
        // bits agree are the same observation and are unioned, which is the
        // same merge the driver does between nodes, applied within a circuit.
        struct Branch {
            std::vector<PauliSetBDD>                 by_t;
            std::map<std::string, std::vector<bool>> bits;
        };

        std::vector<Branch> branches;
        branches.push_back(Branch{reset_ancillas(states_[id], layout), {}});
        for (const auto& reg : program.bits) {
            branches.front().bits.emplace(reg.name, std::vector<bool>(reg.width, false));
        }

        for (const auto& instruction : program.instructions) {
            switch (instruction.kind) {
                case QasmInstruction::Kind::Barrier:
                    break;

                case QasmInstruction::Kind::Gate:
                    for (auto& branch : branches) {
                        apply_gate(branch.by_t, instruction, layout);
                    }
                    break;

                case QasmInstruction::Kind::Reset: {
                    const std::vector<int> target{qubit_of(instruction.qubits[0], layout)};
                    for (auto& branch : branches) {
                        for (auto& level : branch.by_t) level = level.reset_qubits(target);
                    }
                    break;
                }

                case QasmInstruction::Kind::Measure:
                    branches = split_on_measurement(branches, instruction, layout);
                    break;
            }
        }

        std::vector<std::pair<Outcome, StateId>> out;
        for (auto& branch : branches) {
            // The ancillas are done; their outcome is recorded, and anything
            // left on them cannot influence the next circuit.
            std::vector<PauliSetBDD> cleaned;
            cleaned.reserve(branch.by_t.size());
            for (auto& level : branch.by_t) cleaned.push_back(level.reset_qubits(layout.ancillas));
            if (all_empty(cleaned)) continue;

            Outcome outcome;
            outcome.syndrome = read_register(branch.bits, records.syndrome, circuit);
            if (records.flag) outcome.flag = read_register(branch.bits, *records.flag, circuit);
            states_.push_back(std::move(cleaned));
            out.emplace_back(std::move(outcome), states_.size() - 1);
        }
        step_cache_.emplace(key, out);
        return out;
    }


    std::optional<Failure> check(StateId id) override {
        ++check_calls_;
        const auto print = fingerprint(states_[id]);
        if (const auto found = check_cache_.find(print); found != check_cache_.end()) {
            ++check_hits_;
            return found->second;
        }
        const auto answer = check_uncached(id);
        check_cache_.emplace(print, answer);
        return answer;
    }

    std::string statistics() const override {
        std::ostringstream out;
        out << "dd backend      : " << step_calls_ << " step(s), " << step_hits_ << " from cache"
            << " (" << percent(step_hits_, step_calls_) << "%); " << check_calls_
            << " check(s), " << check_hits_ << " from cache (" << percent(check_hits_, check_calls_)
            << "%)\n"
            << "distinct states : " << check_cache_.size() << " checked, " << step_cache_.size()
            << " stepped";
        return out.str();
    }

private:
    static int percent(std::size_t part, std::size_t whole) {
        return whole == 0 ? 0 : static_cast<int>(100.0 * static_cast<double>(part) /
                                                 static_cast<double>(whole));
    }

    std::optional<Failure> check_uncached(StateId id) {
        const auto& state = states_[id];
        for (std::size_t t = 0; t < state.size(); ++t) {
            if (state[t].is_empty()) continue;
            const auto hit = pbdd::find_undetectable_logical_pair(*code_, state[t]);
            if (!hit.found) continue;

            std::ostringstream detail;
            detail << hit.witness_1 << " and " << hit.witness_2 << " share syndrome "
                   << hit.syndrome << " but differ logically (" << hit.logical_a << " vs "
                   << hit.logical_b << ")";
            return Failure{static_cast<int>(t), detail.str()};
        }
        return std::nullopt;
    }

public:
    std::string describe(StateId id) const override {
        std::ostringstream out;
        for (std::size_t t = 0; t < states_[id].size(); ++t) {
            if (t != 0) out << ' ';
            out << 't' << t << '=' << states_[id][t].size();
        }
        return out.str();
    }

private:
    void release() {
        if (!owns_session_) return;
        owns_session_ = false;
        try {
            step_cache_.clear();
            check_cache_.clear();
            states_.clear();
            code_.reset();
            PauliSetBDD::done();
        } catch (...) {
            // Nothing useful to do while unwinding.
        }
    }

    std::vector<PauliSetBDD> fresh_state() const {
        std::vector<PauliSetBDD> state;
        state.reserve(static_cast<std::size_t>(tau_) + 1);
        state.push_back(PauliSetBDD::single(std::string(PauliSetBDD::num_qubits(), 'I')));
        for (int t = 1; t <= tau_; ++t) state.push_back(PauliSetBDD::empty());
        return state;
    }

    static bool all_empty(const std::vector<PauliSetBDD>& state) {
        for (const auto& level : state) {
            if (!level.is_empty()) return false;
        }
        return true;
    }

    // A state's identity, exact because BuDDy shares equal diagrams.
    using Fingerprint = std::vector<int>;
    using CacheKey    = std::pair<Fingerprint, std::string>;

    static Fingerprint fingerprint(const std::vector<PauliSetBDD>& state) {
        Fingerprint print;
        print.reserve(state.size());
        for (const auto& level : state) print.push_back(level.raw().id());
        return print;
    }

    void grow_to(int qubits) {
        if (qubits <= allocated_) return;
        PauliSetBDD::grow(qubits);
        // Root ids survive a grow but the sets they stand for do not: the new
        // variables are unconstrained until they are pinned below. Rather than
        // reason about whether a stale key could still be hit, drop them.
        step_cache_.clear();
        check_cache_.clear();
        // Growing leaves the new variables unconstrained, so every set would
        // silently gain 4^added configurations on them. Pin them at once.
        std::vector<int> added;
        for (int q = allocated_; q < qubits; ++q) added.push_back(q);
        for (auto& state : states_) {
            for (auto& level : state) level = level.reset_qubits(added);
        }
        allocated_ = qubits;
    }

    const QasmProgram& program_for(const CircuitRef& circuit) {
        const std::string key = circuit.qasm.string();
        if (const auto found = programs_.find(key); found != programs_.end()) {
            return found->second;
        }
        return programs_.emplace(key, parse_qasm_file(circuit.qasm)).first->second;
    }

    Layout layout_for(const QasmProgram& program, const CircuitRef& circuit) const {
        const auto width = [&](const std::string& name, const char* role) {
            if (!program.has_qubit_register(name)) {
                throw std::runtime_error("dd backend: " + circuit.qasm.string() + " has no " +
                                         role + " register '" + name + "'");
            }
            return static_cast<int>(program.qubit_width(name));
        };

        Layout layout;
        const int data = width(circuit.data_qubits, "qd");
        if (data != data_qubits_) {
            throw std::runtime_error("dd backend: " + circuit.qasm.string() + " declares " +
                                     std::to_string(data) + " data qubits but the code has " +
                                     std::to_string(data_qubits_));
        }
        layout.base[circuit.data_qubits] = 0;
        int next = data_qubits_;

        const auto place = [&](const std::string& name, const char* role) {
            const int n = width(name, role);
            layout.base[name] = next;
            for (int i = 0; i < n; ++i) layout.ancillas.push_back(next + i);
            next += n;
        };
        place(circuit.syndrome_qubits, "qm");
        if (circuit.flag_qubits) place(*circuit.flag_qubits, "qf");

        layout.total = next;
        return layout;
    }

    static int qubit_of(const QubitRef& ref, const Layout& layout) {
        const auto found = layout.base.find(ref.reg);
        if (found == layout.base.end()) {
            throw std::runtime_error("dd backend: register '" + ref.reg +
                                     "' has no role in this circuit; the protocol names only "
                                     "qd, qm and qf");
        }
        return found->second + static_cast<int>(ref.index);
    }

    std::vector<PauliSetBDD> reset_ancillas(const std::vector<PauliSetBDD>& state,
                                            const Layout& layout) const {
        std::vector<PauliSetBDD> out;
        out.reserve(state.size());
        for (const auto& level : state) out.push_back(level.reset_qubits(layout.ancillas));
        return out;
    }

    void apply_gate(std::vector<PauliSetBDD>& by_t, const QasmInstruction& instruction,
                    const Layout& layout) const {
        const std::string& gate = instruction.gate;
        if (gate == "i" || gate == "id") return;

        if (instruction.qubits.size() == 1) {
            const int q = qubit_of(instruction.qubits[0], layout);
            for (auto& level : by_t) {
                if (gate == "x") level = level.apply_X(q);
                else if (gate == "z") level = level.apply_Z(q);
                // x, y and z conjugate to themselves up to a phase, so they
                // leave the error set alone; apply_X / apply_Z say so.
                else if (gate == "y") level = level.apply_X(q).apply_Z(q);
                else if (gate == "h") level = level.apply_H(q);
                else if (gate == "s" || gate == "sdg") level = level.apply_S(q);
                else throw std::runtime_error("dd backend: unsupported gate '" + gate + "'");
            }
            return;
        }

        const int control = qubit_of(instruction.qubits[0], layout);
        const int target  = qubit_of(instruction.qubits[1], layout);
        for (auto& level : by_t) {
            if (gate == "cx") level = level.apply_CX(control, target);
            else if (gate == "cy") level = level.apply_CY(control, target);
            else if (gate == "cz") level = level.apply_CZ(control, target);
            else throw std::runtime_error("dd backend: unsupported gate '" + gate + "'");
        }

        // Every two-qubit gate is a fault location. Single-qubit gates are
        // taken to be fault free, which is the model the protocols assume.
        //
        // All spawns read the post-gate state before any of them is merged in,
        // so one gate adds at most one fault to any lineage; feeding the merged
        // result back would let a single location fault twice.
        std::vector<PauliSetBDD> spawn;
        spawn.reserve(static_cast<std::size_t>(tau_));
        for (int t = 0; t < tau_; ++t) {
            spawn.push_back(by_t[static_cast<std::size_t>(t)]
                                .multiply_by_all_paulis_on(control, target));
        }
        for (int t = 0; t < tau_; ++t) {
            by_t[static_cast<std::size_t>(t) + 1] =
                by_t[static_cast<std::size_t>(t) + 1] | spawn[static_cast<std::size_t>(t)];
        }
    }

    // A Z-basis measurement reads the x component: X and Y anticommute with Z
    // and flip the outcome, I and Z do not. Splitting on that variable is the
    // whole of what a measurement does to the set.
    template <typename BranchType>
    std::vector<BranchType> split_on_measurement(std::vector<BranchType>& branches,
                                                 const QasmInstruction& instruction,
                                                 const Layout& layout) const {
        const int q = qubit_of(instruction.qubits[0], layout);
        const bdd zero = bdd_nithvar(PauliSetBDD::xvar(q));
        const bdd one  = bdd_ithvar(PauliSetBDD::xvar(q));

        // Keyed by the classical bits so far: two branches that agree on
        // everything measured are indistinguishable and must be unioned.
        std::map<std::string, BranchType> merged;
        for (auto& branch : branches) {
            for (const int bit : {0, 1}) {
                BranchType next = branch;
                next.bits[instruction.target.reg][instruction.target.index] = bit != 0;

                bool empty = true;
                for (auto& level : next.by_t) {
                    level = level & PauliSetBDD(bit == 0 ? zero : one);
                    empty = empty && level.is_empty();
                }
                if (empty) continue;

                const std::string key = signature(next.bits);
                if (const auto found = merged.find(key); found != merged.end()) {
                    for (std::size_t t = 0; t < next.by_t.size(); ++t) {
                        found->second.by_t[t] = found->second.by_t[t] | next.by_t[t];
                    }
                } else {
                    merged.emplace(key, std::move(next));
                }
            }
        }

        std::vector<BranchType> out;
        out.reserve(merged.size());
        for (auto& [key, branch] : merged) out.push_back(std::move(branch));
        return out;
    }

    static std::string signature(const std::map<std::string, std::vector<bool>>& bits) {
        std::string key;
        for (const auto& [name, values] : bits) {
            key += name;
            key += ':';
            for (const bool bit : values) key += bit ? '1' : '0';
            key += ';';
        }
        return key;
    }

    // Which classical register each role's outcome lands in. The protocol does
    // not say -- and does not need to: a measurement statement already binds a
    // classical register to the qubit it reads, so naming the syndrome and flag
    // ancillas fixes the answer. Deriving it also rules out the failure a
    // declaration invites, where the name given and the name the circuit
    // actually writes to disagree and the syndrome is silently read off the
    // flag register.
    struct Records {
        std::string                syndrome;
        std::optional<std::string> flag;
    };

    const Records& records_for(const CircuitRef& circuit, const QasmProgram& program) {
        const std::string key = circuit.qasm.string();
        if (const auto found = records_.find(key); found != records_.end()) return found->second;

        const auto target_of = [&](const std::string& qubits, const char* role) {
            std::optional<std::string> found;
            for (const auto& instruction : program.instructions) {
                if (instruction.kind != QasmInstruction::Kind::Measure) continue;
                if (instruction.qubits.at(0).reg != qubits) continue;
                if (found && *found != instruction.target.reg) {
                    throw std::runtime_error("dd backend: " + circuit.qasm.string() +
                                             " measures " + role + " register '" + qubits +
                                             "' into both '" + *found + "' and '" +
                                             instruction.target.reg +
                                             "'; a role must land in one register");
                }
                found = instruction.target.reg;
            }
            return found;
        };

        Records records;
        const auto syndrome = target_of(circuit.syndrome_qubits, "qm");
        if (!syndrome) {
            throw std::runtime_error("dd backend: " + circuit.qasm.string() +
                                     " never measures its qm register '" +
                                     circuit.syndrome_qubits + "', so there is no syndrome");
        }
        records.syndrome = *syndrome;
        if (circuit.flag_qubits) {
            records.flag = target_of(*circuit.flag_qubits, "qf");
            if (!records.flag) {
                throw std::runtime_error("dd backend: " + circuit.qasm.string() +
                                         " never measures its qf register '" +
                                         *circuit.flag_qubits + "', so there is no flag");
            }
        }
        return records_.emplace(key, std::move(records)).first->second;
    }

    static std::vector<bool> read_register(
        const std::map<std::string, std::vector<bool>>& bits, const std::string& name,
        const CircuitRef& circuit) {
        const auto found = bits.find(name);
        if (found == bits.end()) {
            throw std::runtime_error("dd backend: " + circuit.qasm.string() +
                                     " declares no classical register '" + name + "'");
        }
        return found->second;
    }

    int  data_qubits_  = 0;
    int  tau_          = 0;
    int  allocated_    = 0;
    bool owns_session_ = false;

    std::map<CacheKey, std::vector<std::pair<Outcome, StateId>>> step_cache_;
    std::map<Fingerprint, std::optional<Failure>>                check_cache_;
    std::size_t step_calls_ = 0, step_hits_ = 0, check_calls_ = 0, check_hits_ = 0;

    std::unique_ptr<pbdd::StabilizerCode>      code_;
    std::map<std::string, QasmProgram>         programs_;
    std::map<std::string, Records>             records_;
    std::vector<std::vector<PauliSetBDD>>      states_;
};

} // namespace

std::unique_ptr<Backend> make_dd_backend() { return std::make_unique<DdBackend>(); }

} // namespace ftec
