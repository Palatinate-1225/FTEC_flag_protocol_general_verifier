#include "spbdd_backend.hpp"

#include "ftec/qasm.hpp"

#include <spbdd/spbdd.hpp>

// cudd.h uses size_t and FILE but includes neither header itself, so these two
// lines are load-bearing and must come first. (SPBDD's own sources say the
// same thing; this file needs CUDD directly only for the reordering knobs
// SPBDD does not expose.)
#include <cstddef>
#include <cstdio>

#include <cudd.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ftec {

namespace {

using spbdd::PauliSet;
using spbdd::PauliSpace;

// The reordering methods CUDD offers, by the name this backend accepts.
//
// EXACT, GENETIC and ANNEALING are here for completeness, but a [[17,1,5]]
// protocol runs to 42 variables and EXACT is exponential in that: they are for
// small circuits, not for the protocols in protocols/.
//
// LINEAR and LINEAR_CONVERGE are *refused*, and the reason is worth stating
// because it is not "they were slow". Every other method permutes levels, and
// SPBDD is built on that: paulispace.hpp says "CUDD may reorder levels freely;
// all code here is written against variable numbers, which never change".
// Linear sifting breaks precisely that -- it also applies linear
// transformations, replacing a variable by the XOR of two, so a variable
// number stops denoting what it denoted. The cubes SPBDD builds by variable
// number for its quantifications are then not the cubes it thinks they are,
// and CUDD says so, in one of two ways depending on which goes wrong first:
//
//   linear       cuddGarbageCollect: problem in table 5, dead count != deleted
//   linear_conv  Error: Can only abstract positive cubes
//
// The first is an abort. Neither is a diagnosis anybody should have to make
// twice, so the combination is rejected here rather than at run time.
struct ReorderMethod {
    const char*         name;
    Cudd_ReorderingType type;
    bool                supported;
};

constexpr ReorderMethod kMethods[] = {
    {"none",            CUDD_REORDER_NONE,            true},
    {"window2",         CUDD_REORDER_WINDOW2,         true},
    {"window3",         CUDD_REORDER_WINDOW3,         true},
    {"window4",         CUDD_REORDER_WINDOW4,         true},
    {"window2_conv",    CUDD_REORDER_WINDOW2_CONV,    true},
    {"window3_conv",    CUDD_REORDER_WINDOW3_CONV,    true},
    {"window4_conv",    CUDD_REORDER_WINDOW4_CONV,    true},
    {"random",          CUDD_REORDER_RANDOM,          true},
    {"random_pivot",    CUDD_REORDER_RANDOM_PIVOT,    true},
    {"sift",            CUDD_REORDER_SIFT,            true},
    {"sift_conv",       CUDD_REORDER_SIFT_CONVERGE,   true},
    {"symm_sift",       CUDD_REORDER_SYMM_SIFT,       true},
    {"symm_sift_conv",  CUDD_REORDER_SYMM_SIFT_CONV,  true},
    {"group_sift",      CUDD_REORDER_GROUP_SIFT,      true},
    {"group_sift_conv", CUDD_REORDER_GROUP_SIFT_CONV, true},
    {"lazy_sift",       CUDD_REORDER_LAZY_SIFT,       true},
    {"linear",          CUDD_REORDER_LINEAR,          false},
    {"linear_conv",     CUDD_REORDER_LINEAR_CONVERGE, false},
    {"annealing",       CUDD_REORDER_ANNEALING,       true},
    {"genetic",         CUDD_REORDER_GENETIC,         true},
    {"exact",           CUDD_REORDER_EXACT,           true},
};

// Where each register of a circuit lands in the global qubit numbering; the
// same convention the dd backend uses, and for the same reason. Data qubits
// are fixed at 0..n-1 and carry the encoded state across the whole protocol;
// ancillas sit above them and are laid out per circuit, which is sound only
// because every step begins and ends with them at identity.
struct Layout {
    std::map<std::string, int> base;     // register name -> first global qubit
    std::vector<int>           ancillas; // every ancilla qubit, for the resets
    int                        total = 0;
};

std::string bits_to_string(const std::vector<bool>& bits) {
    std::string out;
    out.reserve(bits.size());
    for (const bool bit : bits) out += bit ? '1' : '0';
    return out;
}

class SpbddBackend : public Backend {
public:
    explicit SpbddBackend(SpbddReorder reorder) : reorder_(std::move(reorder)) {}

    void begin(const fpdl::CodeSpec& code, int tau) override {
        if (code.generators.empty()) {
            throw std::runtime_error(
                "spbdd backend: the protocol declares no stabilizer generators");
        }
        release();
        data_qubits_ = code.n;
        tau_         = tau;
        generators_  = code.generators;

        // ManagerConfig's own reordering flag stays off and the choice is made
        // below instead, so that "sift" goes through exactly the same call as
        // every other method and none of them is a special case.
        space_.emplace(data_qubits_, spbdd::ManagerConfig{});
        apply_reordering();
        allocated_ = data_qubits_;

        // StabilizerCode does the one-off linear algebra and says so clearly
        // if the generators are not a stabilizer group.
        rebuild_code();

        states_.push_back(fresh_state());
    }

    StateId initial_state() override { return 0; }

    std::vector<std::pair<Outcome, StateId>> step(StateId id,
                                                  const CircuitRef& circuit) override {
        ++step_calls_;
        // Two different measurement records often leave the same set of errors
        // behind -- the record is what the decoder saw, not what survived --
        // and stepping depends only on the set. CUDD hash-conses and keeps its
        // diagrams canonical, so equal sets are literally the same node and a
        // key of their addresses is exact rather than a heuristic.
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
                        for (auto& level : branch.by_t) level = level.reset(target);
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
            std::vector<PauliSet> cleaned;
            cleaned.reserve(branch.by_t.size());
            for (auto& level : branch.by_t) cleaned.push_back(level.reset(layout.ancillas));
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

    std::string describe(StateId id) const override {
        std::ostringstream out;
        for (std::size_t t = 0; t < states_[id].size(); ++t) {
            if (t != 0) out << ' ';
            out << 't' << t << '=' << states_[id][t].size();
        }
        return out.str();
    }

    std::string statistics() const override {
        std::ostringstream out;
        out << "spbdd backend   : " << step_calls_ << " step(s), " << step_hits_
            << " from cache (" << percent(step_hits_, step_calls_) << "%); " << check_calls_
            << " check(s), " << check_hits_ << " from cache ("
            << percent(check_hits_, check_calls_) << "%)\n"
            << "distinct states : " << check_cache_.size() << " checked, " << step_cache_.size()
            << " stepped";
        if (space_) {
            const spbdd::Manager& manager = space_->manager();
            DdManager*            dd      = manager.raw();
            out << "\ndiagram         : " << manager.live_nodes() << " live node(s), peak "
                << manager.peak_nodes() << ", " << manager.memory_in_use() << " bytes, "
                << space_->n_qubits() << " qubits"
                << "\nreordering      : " << reorder_.method;
            if (reorder_.threshold > 0) out << " @" << reorder_.threshold;
            // How often it actually fired and what that cost, which is the
            // number the method names alone do not tell you.
            out << ", " << Cudd_ReadReorderings(dd) << " run(s), "
                << Cudd_ReadReorderingTime(dd) << " ms";

            // CUDD's own operator cache, which is the thing a reordering
            // costs *besides* the time it spends reordering: every reorder
            // flushes it, and everything memoised in it has to be recomputed.
            // Whether that is where the time goes is not something to guess
            // at when the counters are right here.
            const double lookups = static_cast<double>(Cudd_ReadCacheLookUps(dd));
            const double hits    = static_cast<double>(Cudd_ReadCacheHits(dd));
            out << "\ncudd cache      : " << static_cast<std::uintmax_t>(lookups)
                << " lookup(s), " << static_cast<std::uintmax_t>(hits) << " hit(s) ("
                << (lookups > 0 ? static_cast<int>(100.0 * hits / lookups) : 0) << "%), "
                << Cudd_ReadGarbageCollections(dd) << " gc(s), "
                << Cudd_ReadGarbageCollectionTime(dd) << " ms";
        }
        return out.str();
    }

private:
    // One classical outcome so far, and the error sets that can accompany it.
    struct Branch {
        std::vector<PauliSet>                    by_t;
        std::map<std::string, std::vector<bool>> bits;
    };

    static int percent(std::size_t part, std::size_t whole) {
        return whole == 0 ? 0 : static_cast<int>(100.0 * static_cast<double>(part) /
                                                 static_cast<double>(whole));
    }

    // Reach past SPBDD to the DdManager it wraps. Everything here is a CUDD
    // setting SPBDD's public API does not expose; Manager::raw() exists for
    // exactly this and the library documents it as an escape hatch.
    void apply_reordering() {
        DdManager* dd = space_->manager().raw();

        const Cudd_ReorderingType type = method_type(reorder_.method);
        if (type == CUDD_REORDER_NONE) {
            Cudd_AutodynDisable(dd);
        } else {
            Cudd_AutodynEnable(dd, type);
        }

        // Only the *first* trigger is set here. CUDD recomputes the next one
        // after every reordering as roughly twice the size the diagram ended
        // up at, so this shifts the whole schedule rather than fixing it, and
        // a large value is "reorder rarely", not "reorder once".
        if (reorder_.threshold > 0) {
            Cudd_SetNextReordering(dd, static_cast<unsigned>(reorder_.threshold));
        }
    }

    static Cudd_ReorderingType method_type(const std::string& name) {
        for (const auto& method : kMethods) {
            if (name != method.name) continue;
            if (!method.supported) {
                throw std::runtime_error(
                    "spbdd backend: reordering method '" + name +
                    "' also applies linear transformations, which change what a variable "
                    "means. SPBDD addresses its variables by number and assumes only the "
                    "level order moves, so this corrupts its quantification cubes (CUDD "
                    "aborts in cuddGarbageCollect, or refuses to abstract a non-positive "
                    "cube). Every other method only permutes levels and is safe.");
            }
            return method.type;
        }
        throw std::runtime_error("spbdd backend: unknown reordering method '" + name + "'");
    }

    void release() {
        step_cache_.clear();
        check_cache_.clear();
        programs_.clear();
        records_.clear();
        states_.clear();
        code_.reset();
        space_.reset();
        allocated_ = 0;
    }

    // The code is stated over the data qubits, the sets live over the data
    // qubits *and* the ancillas, and SPBDD requires both to sit in one space.
    // Padding each generator with identities on the ancillas is the honest way
    // to say so: the group becomes S tensor {I}, its normaliser leaves the
    // ancillas free, and the extra logical qubits the padding introduces
    // contribute nothing to a signature, because every set this is ever asked
    // about has its ancillas back at identity. What the query decides is
    // therefore exactly the data-qubit question.
    void rebuild_code() {
        const std::string pad(static_cast<std::size_t>(allocated_ - data_qubits_), 'I');
        std::vector<std::string> padded;
        padded.reserve(generators_.size());
        for (const auto& generator : generators_) padded.push_back(generator + pad);
        code_ = std::make_unique<spbdd::StabilizerCode>(*space_, padded);
    }

    std::vector<PauliSet> fresh_state() const {
        std::vector<PauliSet> state;
        state.reserve(static_cast<std::size_t>(tau_) + 1);
        state.push_back(space_->identity());
        for (int t = 1; t <= tau_; ++t) state.push_back(space_->empty());
        return state;
    }

    static bool all_empty(const std::vector<PauliSet>& state) {
        for (const auto& level : state) {
            if (!level.is_empty()) return false;
        }
        return true;
    }

    // A state's identity. CUDD keeps its diagrams canonical and shares equal
    // subgraphs, so equal sets are the same node and the node address is an
    // exact key. It stays valid because states_ owns a reference to every node
    // it names and never drops one -- apart from grow_to(), which clears the
    // caches for exactly that reason.
    using Fingerprint = std::vector<std::uintptr_t>;
    using CacheKey    = std::pair<Fingerprint, std::string>;

    static Fingerprint fingerprint(const std::vector<PauliSet>& state) {
        Fingerprint print;
        print.reserve(state.size());
        for (const auto& level : state) {
            print.push_back(reinterpret_cast<std::uintptr_t>(level.bdd().node()));
        }
        return print;
    }

    void grow_to(int qubits) {
        if (qubits <= allocated_) return;
        space_->grow_to(qubits);
        // The sets the cached keys named are about to be replaced and their
        // nodes freed; CUDD may hand those addresses back out for something
        // else, so no key that mentions one can be trusted afterwards.
        step_cache_.clear();
        check_cache_.clear();
        // Growing leaves the new variables unconstrained, so every set would
        // silently gain 4^added configurations on them. Pin them at once.
        std::vector<int> added;
        for (int q = allocated_; q < qubits; ++q) added.push_back(q);
        for (auto& state : states_) {
            for (auto& level : state) level = level.reset(added);
        }
        allocated_ = qubits;
        // The generators have to be restated over the wider space.
        rebuild_code();
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
                throw std::runtime_error("spbdd backend: " + circuit.qasm.string() + " has no " +
                                         role + " register '" + name + "'");
            }
            return static_cast<int>(program.qubit_width(name));
        };

        Layout layout;
        const int data = width(circuit.data_qubits, "qd");
        if (data != data_qubits_) {
            throw std::runtime_error("spbdd backend: " + circuit.qasm.string() + " declares " +
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
            throw std::runtime_error("spbdd backend: register '" + ref.reg +
                                     "' has no role in this circuit; the protocol names only "
                                     "qd, qm and qf");
        }
        return found->second + static_cast<int>(ref.index);
    }

    static std::vector<PauliSet> reset_ancillas(const std::vector<PauliSet>& state,
                                                const Layout& layout) {
        std::vector<PauliSet> out;
        out.reserve(state.size());
        for (const auto& level : state) out.push_back(level.reset(layout.ancillas));
        return out;
    }

    void apply_gate(std::vector<PauliSet>& by_t, const QasmInstruction& instruction,
                    const Layout& layout) const {
        const std::string& gate = instruction.gate;
        if (gate == "i" || gate == "id") return;

        if (instruction.qubits.size() == 1) {
            const int q = qubit_of(instruction.qubits[0], layout);
            for (auto& level : by_t) {
                // x, y and z conjugate to themselves up to a phase, so they
                // leave the error set alone; the library says so itself.
                if (gate == "x") level = level.x(q);
                else if (gate == "y") level = level.y(q);
                else if (gate == "z") level = level.z(q);
                else if (gate == "h") level = level.h(q);
                else if (gate == "s" || gate == "sdg") level = level.s(q);
                else throw std::runtime_error("spbdd backend: unsupported gate '" + gate + "'");
            }
            return;
        }

        const int control = qubit_of(instruction.qubits[0], layout);
        const int target  = qubit_of(instruction.qubits[1], layout);
        for (auto& level : by_t) {
            if (gate == "cx") level = level.cx(control, target);
            else if (gate == "cy") level = level.cy(control, target);
            else if (gate == "cz") level = level.cz(control, target);
            else throw std::runtime_error("spbdd backend: unsupported gate '" + gate + "'");
        }

        // Every two-qubit gate is a fault location. Single-qubit gates are
        // taken to be fault free, which is the model the protocols assume.
        //
        // All spawns read the post-gate state before any of them is merged in,
        // so one gate adds at most one fault to any lineage; feeding the merged
        // result back would let a single location fault twice.
        const std::vector<int> location{control, target};
        std::vector<PauliSet>  spawn;
        spawn.reserve(static_cast<std::size_t>(tau_));
        for (int t = 0; t < tau_; ++t) {
            spawn.push_back(by_t[static_cast<std::size_t>(t)].fault_inject(location));
        }
        for (int t = 0; t < tau_; ++t) {
            by_t[static_cast<std::size_t>(t) + 1] =
                by_t[static_cast<std::size_t>(t) + 1] | spawn[static_cast<std::size_t>(t)];
        }
    }

    // Measuring splits the set rather than changing it, and which half an
    // error lands in is the outcome the decoder reads. An empty half is an
    // outcome that cannot occur under these errors.
    std::vector<Branch> split_on_measurement(std::vector<Branch>& branches,
                                             const QasmInstruction& instruction,
                                             const Layout& layout) const {
        const int q = qubit_of(instruction.qubits[0], layout);

        // Keyed by the classical bits so far: two branches that agree on
        // everything measured are indistinguishable and must be unioned.
        std::map<std::string, Branch> merged;
        for (auto& branch : branches) {
            Branch outcome[2] = {branch, branch};
            bool   empty[2]   = {true, true};
            for (std::size_t t = 0; t < branch.by_t.size(); ++t) {
                const auto split = branch.by_t[t].measure_z(q);
                outcome[0].by_t[t] = split.unflipped;
                outcome[1].by_t[t] = split.flipped;
                empty[0] = empty[0] && split.unflipped.is_empty();
                empty[1] = empty[1] && split.flipped.is_empty();
            }

            for (const int bit : {0, 1}) {
                if (empty[bit]) continue;
                Branch& next = outcome[bit];
                next.bits[instruction.target.reg][instruction.target.index] = bit != 0;

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

        std::vector<Branch> out;
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

    std::optional<Failure> check_uncached(StateId id) {
        const auto& state = states_[id];
        for (std::size_t t = 0; t < state.size(); ++t) {
            if (state[t].is_empty()) continue;
            const auto pair = code_->find_inequivalent_pair(state[t]);
            if (!pair) continue;

            // The witnesses run over the whole space; their ancilla half is
            // identity by construction, so only the data prefix says anything.
            const auto on_data = [this](const std::string& p) {
                return p.substr(0, static_cast<std::size_t>(data_qubits_));
            };
            std::ostringstream detail;
            detail << on_data(pair->first) << " and " << on_data(pair->second)
                   << " share syndrome " << bits_to_string(pair->syndrome)
                   << " but differ logically (" << bits_to_string(pair->signature_first)
                   << " vs " << bits_to_string(pair->signature_second) << ")";
            return Failure{static_cast<int>(t), detail.str()};
        }
        return std::nullopt;
    }

    // Which classical register each role's outcome lands in. The protocol does
    // not say -- and does not need to: a measurement statement already binds a
    // classical register to the qubit it reads, so naming the syndrome and flag
    // ancillas fixes the answer.
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
                    throw std::runtime_error("spbdd backend: " + circuit.qasm.string() +
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
            throw std::runtime_error("spbdd backend: " + circuit.qasm.string() +
                                     " never measures its qm register '" +
                                     circuit.syndrome_qubits + "', so there is no syndrome");
        }
        records.syndrome = *syndrome;
        if (circuit.flag_qubits) {
            records.flag = target_of(*circuit.flag_qubits, "qf");
            if (!records.flag) {
                throw std::runtime_error("spbdd backend: " + circuit.qasm.string() +
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
            throw std::runtime_error("spbdd backend: " + circuit.qasm.string() +
                                     " declares no classical register '" + name + "'");
        }
        return found->second;
    }

    SpbddReorder reorder_;
    int          data_qubits_ = 0;
    int          tau_         = 0;
    int          allocated_   = 0;

    std::map<CacheKey, std::vector<std::pair<Outcome, StateId>>> step_cache_;
    std::map<Fingerprint, std::optional<Failure>>                check_cache_;
    std::size_t step_calls_ = 0, step_hits_ = 0, check_calls_ = 0, check_hits_ = 0;

    std::vector<std::string>               generators_;
    std::optional<PauliSpace>              space_;
    std::unique_ptr<spbdd::StabilizerCode> code_;
    std::map<std::string, QasmProgram>     programs_;
    std::map<std::string, Records>         records_;
    std::vector<std::vector<PauliSet>>     states_;
};

} // namespace

std::vector<std::string> spbdd_reorder_methods() {
    std::vector<std::string> names;
    names.reserve(std::size(kMethods));
    for (const auto& method : kMethods) names.emplace_back(method.name);
    return names;
}

bool is_spbdd_reorder_method(const std::string& name) {
    for (const auto& method : kMethods) {
        if (name == method.name) return true;
    }
    return false;
}

std::unique_ptr<Backend> make_spbdd_backend(const SpbddReorder& reorder) {
    return std::make_unique<SpbddBackend>(reorder);
}

} // namespace ftec
