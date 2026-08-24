#include "sat_backend.hpp"

#include "pauli_gf2.hpp"
#include "stabilizer_gf2.hpp"

#include <cryptominisat5/cryptominisat.h>

#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ftec {

namespace {

// One fault location, identified the same way sat::FaultVar disambiguates
// it: which step() call (tag) and which two-qubit gate within that call's
// circuit (location). Used to build one "did this location fire at all"
// indicator per location for the fault-count cardinality constraint --
// counting individual X/Z sub-variables would count one Y-type fault as two.
struct LocationKey {
    std::size_t tag = 0;
    std::size_t location = 0;
    friend bool operator<(const LocationKey& a, const LocationKey& b) {
        return std::tie(a.tag, a.location) < std::tie(b.tag, b.location);
    }
};

LocationKey location_of(const sat::FaultVar& v) { return {v.tag, v.location}; }

// A boolean fact this path's history has already fixed: `formula`'s XOR must
// equal `value`. Together these are Phi_so_far.
struct FixedOutcome {
    sat::XorTerm formula;
    bool value = false;
};

// Everything a StateId needs to remember: what's already been observed
// (fixed_outcomes) and the symbolic error currently sitting on each data
// qubit (data[i], code-qubit index, independent of any one circuit's
// register name). Both are formulas over fault variables reaching back to
// the very start of the path, so a fresh SAT instance rebuilt from just
// this is self-contained -- no persistent solver state to manage across
// branches of the DFS.
struct SatState {
    std::vector<FixedOutcome> fixed_outcomes;
    std::vector<sat::SymbolicPauli> data;
};

// check() needs two *independent* fault assignments (F1, F2) consistent
// with the same history, to search for an undetectable pair. Retagging
// every fault variable a formula mentions by a fixed offset gives two
// disjoint copies of the same symbolic history without re-running
// propagation: offset 0 is F1, COPY_OFFSET is F2 (chosen far past any
// realistic step() tag count, so the two copies' locations never collide).
constexpr std::size_t COPY_OFFSET = 1'000'000'000;

sat::XorTerm retag(const sat::XorTerm& term, std::size_t offset) {
    sat::XorTerm result;
    for (const auto& v : term) result.insert({v.tag + offset, v.location, v.part});
    return result;
}

// symplectic_product(Q, generator) as a formula, exploiting that
// `generator` is a known constant: each term of the sum is either Q's X- or
// Z-component formula (or dropped entirely), never a fresh variable.
sat::XorTerm symplectic_formula(const std::vector<sat::SymbolicPauli>& Q,
                                const sat::PauliString& generator) {
    sat::XorTerm result;
    for (std::size_t i = 0; i < generator.size(); ++i) {
        if (generator[i].z) result = sat::xor_combine(result, Q[i].x);
        if (generator[i].x) result = sat::xor_combine(result, Q[i].z);
    }
    return result;
}

// Small CryptoMiniSat gadget wrapper: lazy one-variable-per-FaultVar
// allocation plus the handful of Tseitin gates this encoder needs.
class Gadgets {
public:
    explicit Gadgets(CMSat::SATSolver& s) : solver_(s) {}

    CMSat::Lit fresh() {
        solver_.new_var();
        return CMSat::Lit(solver_.nVars() - 1, false);
    }

    CMSat::Lit var_for(const sat::FaultVar& v) {
        auto it = var_ids_.find(v);
        if (it != var_ids_.end()) return CMSat::Lit(it->second, false);
        const CMSat::Lit lit = fresh();
        var_ids_.emplace(v, lit.var());
        return lit;
    }

    // A literal equal to the XOR of `term`'s fault variables (false if
    // `term` is empty, i.e. "no error possible here").
    CMSat::Lit term_lit(const sat::XorTerm& term) {
        if (term.empty()) return false_lit();
        const CMSat::Lit a = fresh();
        std::vector<CMSat::Lit> lits{a};
        for (const auto& v : term) lits.push_back(var_for(v));
        solver_.add_xor_clause(lits, false); // a xor (xor of term) = 0  =>  a = xor of term
        return a;
    }

    // Hard-asserts term's XOR equals `value`.
    void assert_xor_equals(const sat::XorTerm& term, bool value) {
        if (term.empty()) {
            if (value) solver_.add_clause({}); // unsatisfiable: "0 == true"
            return;
        }
        std::vector<CMSat::Lit> lits;
        for (const auto& v : term) lits.push_back(var_for(v));
        solver_.add_xor_clause(lits, value);
    }

    CMSat::Lit and_gate(CMSat::Lit a, CMSat::Lit b) {
        const CMSat::Lit c = fresh();
        solver_.add_clause({~c, a});
        solver_.add_clause({~c, b});
        solver_.add_clause({c, ~a, ~b});
        return c;
    }
    CMSat::Lit or_gate(CMSat::Lit a, CMSat::Lit b) {
        const CMSat::Lit d = fresh();
        solver_.add_clause({~d, a, b});
        solver_.add_clause({d, ~a});
        solver_.add_clause({d, ~b});
        return d;
    }

    // "At most k of xs are true" via a Sinz-style sequential-counter
    // register, built from and_gate/or_gate above (see backends/sat's
    // predecessor design notes; not clause-minimal, but that's a later
    // optimization, not a correctness concern).
    void assert_at_most(const std::vector<CMSat::Lit>& xs, std::size_t k) {
        const std::size_t n = xs.size();
        if (n <= k) return;
        std::vector<std::vector<CMSat::Lit>> s(n + 1, std::vector<CMSat::Lit>(k + 2, false_lit()));
        s[0][0] = true_lit();
        for (std::size_t i = 1; i <= n; ++i) {
            s[i][0] = true_lit();
            for (std::size_t j = 1; j <= k + 1; ++j) {
                const CMSat::Lit reached_before = s[i - 1][j];
                const CMSat::Lit newly_reached = and_gate(xs[i - 1], s[i - 1][j - 1]);
                s[i][j] = or_gate(reached_before, newly_reached);
            }
        }
        solver_.add_clause({~s[n][k + 1]});
    }

    // "Exactly k of xs are true": the same counter, asserting both ends
    // (s[n][k] and not s[n][k+1]) instead of only the upper one.
    void assert_exactly(const std::vector<CMSat::Lit>& xs, std::size_t k) {
        const std::size_t n = xs.size();
        if (k > n) {
            solver_.add_clause({}); // impossible: can't have more true than there are literals.
            return;
        }
        std::vector<std::vector<CMSat::Lit>> s(n + 1, std::vector<CMSat::Lit>(k + 2, false_lit()));
        s[0][0] = true_lit();
        for (std::size_t i = 1; i <= n; ++i) {
            s[i][0] = true_lit();
            for (std::size_t j = 1; j <= k + 1; ++j) {
                const CMSat::Lit reached_before = s[i - 1][j];
                const CMSat::Lit newly_reached = and_gate(xs[i - 1], s[i - 1][j - 1]);
                s[i][j] = or_gate(reached_before, newly_reached);
            }
        }
        solver_.add_clause({s[n][k]});
        solver_.add_clause({~s[n][k + 1]});
    }

    CMSat::Lit true_lit() {
        if (!true_lit_) {
            true_lit_ = fresh();
            solver_.add_clause({*true_lit_});
        }
        return *true_lit_;
    }
    CMSat::Lit false_lit() { return ~true_lit(); }

private:
    CMSat::SATSolver& solver_;
    std::map<sat::FaultVar, uint32_t> var_ids_;
    std::optional<CMSat::Lit> true_lit_;
};

} // namespace

class SatBackend : public Backend {
public:
    void begin(const fpdl::CodeSpec& code, int tau) override {
        if (code.generators.empty()) {
            throw std::runtime_error("sat backend: the protocol declares no stabilizer generators");
        }
        code_ = code;
        tau_ = tau;
        n_ = static_cast<std::size_t>(code.n);
        stabilizer_ = sat::build_stabilizer_code(code);
        states_.clear();
        states_.push_back(SatState{{}, std::vector<sat::SymbolicPauli>(n_)});
        next_tag_ = 0;
        step_calls_ = 0;
    }

    StateId initial_state() override { return 0; }

    std::vector<std::pair<Outcome, StateId>> step(StateId id, const CircuitRef& circuit) override {
        ++step_calls_;
        // By value, not reference: the AllSAT loop below pushes new states
        // onto states_ as it finds them, which can reallocate the vector
        // and would dangle a reference held across those pushes.
        const SatState prev = states_[id];
        const auto& program = program_for(circuit);

        if (!program.has_qubit_register(circuit.data_qubits)) {
            throw std::runtime_error("sat backend: " + circuit.qasm.string() + " has no qd register '" +
                                     circuit.data_qubits + "'");
        }
        if (program.qubit_width(circuit.data_qubits) != n_) {
            throw std::runtime_error("sat backend: " + circuit.qasm.string() + " declares " +
                                     std::to_string(program.qubit_width(circuit.data_qubits)) +
                                     " data qubits but the code has " + std::to_string(n_));
        }

        std::map<sat::Wire, sat::SymbolicPauli> initial;
        for (std::size_t i = 0; i < n_; ++i) initial[{circuit.data_qubits, i}] = prev.data[i];

        const std::size_t tag = next_tag_++;
        const auto result = sat::propagate(program, initial, tag);

        // Bucket each recorded outcome by which ancilla register it measured
        // (qm: -> syndrome, qf: -> flag), in program order, since
        // sat::propagate doesn't itself know which role a wire plays.
        std::map<std::size_t, sat::XorTerm> syn_bits, flag_bits;
        std::size_t measure_i = 0;
        for (const auto& instr : program.instructions) {
            if (instr.kind != QasmInstruction::Kind::Measure) continue;
            const auto& outcome = result.outcomes.at(measure_i++);
            const std::string& measured_reg = instr.qubits.at(0).reg;
            if (measured_reg == circuit.syndrome_qubits) {
                syn_bits[outcome.creg.index] = outcome.formula;
            } else if (circuit.flag_qubits && measured_reg == *circuit.flag_qubits) {
                flag_bits[outcome.creg.index] = outcome.formula;
            } else {
                throw std::runtime_error("sat backend: " + circuit.qasm.string() +
                                         " measures a qubit outside qm/qf ('" + measured_reg + "')");
            }
        }
        const std::size_t syn_w = syn_bits.empty() ? 0 : syn_bits.rbegin()->first + 1;
        const std::size_t flag_w = flag_bits.empty() ? 0 : flag_bits.rbegin()->first + 1;
        std::vector<sat::XorTerm> syn_formula(syn_w), flag_formula(flag_w);
        for (auto& [i, f] : syn_bits) syn_formula[i] = std::move(f);
        for (auto& [i, f] : flag_bits) flag_formula[i] = std::move(f);

        std::vector<sat::SymbolicPauli> new_data(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            const auto it = result.final_state.find({circuit.data_qubits, i});
            if (it != result.final_state.end()) new_data[i] = it->second;
        }

        // --- Build the SAT instance for this step's outcome enumeration ---
        CMSat::SATSolver solver;
        Gadgets g(solver);

        for (const auto& fixed : prev.fixed_outcomes) g.assert_xor_equals(fixed.formula, fixed.value);

        std::set<LocationKey> locations;
        const auto collect = [&](const sat::XorTerm& t) {
            for (const auto& v : t) locations.insert(location_of(v));
        };
        for (const auto& fixed : prev.fixed_outcomes) collect(fixed.formula);
        for (const auto& p : prev.data) { collect(p.x); collect(p.z); }
        for (const auto& f : syn_formula) collect(f);
        for (const auto& f : flag_formula) collect(f);
        for (const auto& p : new_data) { collect(p.x); collect(p.z); }

        std::vector<CMSat::Lit> active;
        active.reserve(locations.size());
        for (const auto& loc : locations) {
            const CMSat::Lit x1 = g.var_for({loc.tag, loc.location, sat::FaultPart::XFirst});
            const CMSat::Lit z1 = g.var_for({loc.tag, loc.location, sat::FaultPart::ZFirst});
            const CMSat::Lit x2 = g.var_for({loc.tag, loc.location, sat::FaultPart::XSecond});
            const CMSat::Lit z2 = g.var_for({loc.tag, loc.location, sat::FaultPart::ZSecond});
            active.push_back(g.or_gate(g.or_gate(x1, z1), g.or_gate(x2, z2)));
        }
        g.assert_at_most(active, static_cast<std::size_t>(tau_));

        std::vector<CMSat::Lit> target;
        target.reserve(syn_w + flag_w);
        for (const auto& f : syn_formula) target.push_back(g.term_lit(f));
        for (const auto& f : flag_formula) target.push_back(g.term_lit(f));

        // --- AllSAT: enumerate every reachable combination of target bits ---
        std::vector<std::pair<Outcome, StateId>> out;
        for (;;) {
            const auto solved = solver.solve();
            if (solved != CMSat::l_True) break;
            const auto& model = solver.get_model();

            std::vector<bool> bits;
            bits.reserve(target.size());
            for (const auto& lit : target) bits.push_back(model.at(lit.var()) == CMSat::l_True);

            Outcome outcome;
            outcome.syndrome.assign(bits.begin(), bits.begin() + static_cast<long>(syn_w));
            if (circuit.flag_qubits) {
                outcome.flag.assign(bits.begin() + static_cast<long>(syn_w), bits.end());
            }

            SatState next;
            next.fixed_outcomes = prev.fixed_outcomes;
            for (std::size_t i = 0; i < syn_w; ++i) next.fixed_outcomes.push_back({syn_formula[i], bits[i]});
            for (std::size_t i = 0; i < flag_w; ++i)
                next.fixed_outcomes.push_back({flag_formula[i], bits[syn_w + i]});
            next.data = new_data;

            states_.push_back(std::move(next));
            out.emplace_back(std::move(outcome), states_.size() - 1);

            std::vector<CMSat::Lit> block;
            block.reserve(target.size());
            for (std::size_t i = 0; i < target.size(); ++i) block.push_back(bits[i] ? ~target[i] : target[i]);
            solver.add_clause(block);
        }
        return out;
    }

    // Searches, for t = 0..tau, for two independent fault assignments F1,F2
    // consistent with this state's history (Phi_so_far), each using
    // *exactly* t faults (matching dd's own per-level check -- see the
    // "Cross-t pairs" decision in the design discussion), whose residual
    // data errors' product Q = E(F1)*E(F2) is a nontrivial logical operator
    // (in N(S)\S): Q commutes with every stabilizer generator but
    // anticommutes with at least one logical operator. The smallest such t
    // is reported first, exactly like dd_backend.cpp's check_uncached.
    std::optional<Failure> check(StateId id) override {
        ++check_calls_;
        // By value: see the comment on the same pattern in step().
        const SatState prev = states_[id];

        for (int t = 0; t <= tau_; ++t) {
            CMSat::SATSolver solver;
            Gadgets g(solver);

            std::vector<sat::SymbolicPauli> copy_data[2];
            for (std::size_t c = 0; c < 2; ++c) {
                const std::size_t offset = c == 0 ? 0 : COPY_OFFSET;
                copy_data[c].resize(n_);
                for (std::size_t i = 0; i < n_; ++i) {
                    copy_data[c][i].x = retag(prev.data[i].x, offset);
                    copy_data[c][i].z = retag(prev.data[i].z, offset);
                }
                for (const auto& fixed : prev.fixed_outcomes)
                    g.assert_xor_equals(retag(fixed.formula, offset), fixed.value);

                std::set<LocationKey> locations;
                const auto collect = [&](const sat::XorTerm& term) {
                    for (const auto& v : term) locations.insert(location_of(v));
                };
                for (const auto& fixed : prev.fixed_outcomes) collect(retag(fixed.formula, offset));
                for (const auto& p : prev.data) {
                    collect(retag(p.x, offset));
                    collect(retag(p.z, offset));
                }
                std::vector<CMSat::Lit> active;
                active.reserve(locations.size());
                for (const auto& loc : locations) {
                    const CMSat::Lit x1 = g.var_for({loc.tag, loc.location, sat::FaultPart::XFirst});
                    const CMSat::Lit z1 = g.var_for({loc.tag, loc.location, sat::FaultPart::ZFirst});
                    const CMSat::Lit x2 = g.var_for({loc.tag, loc.location, sat::FaultPart::XSecond});
                    const CMSat::Lit z2 = g.var_for({loc.tag, loc.location, sat::FaultPart::ZSecond});
                    active.push_back(g.or_gate(g.or_gate(x1, z1), g.or_gate(x2, z2)));
                }
                g.assert_exactly(active, static_cast<std::size_t>(t));
            }

            std::vector<sat::SymbolicPauli> Q(n_);
            for (std::size_t i = 0; i < n_; ++i) {
                Q[i].x = sat::xor_combine(copy_data[0][i].x, copy_data[1][i].x);
                Q[i].z = sat::xor_combine(copy_data[0][i].z, copy_data[1][i].z);
            }
            for (const auto& gen : stabilizer_.generators)
                g.assert_xor_equals(symplectic_formula(Q, gen), false);

            std::vector<CMSat::Lit> nontrivial;
            nontrivial.reserve(stabilizer_.logical_operators.size());
            for (const auto& lg : stabilizer_.logical_operators)
                nontrivial.push_back(g.term_lit(symplectic_formula(Q, lg)));
            solver.add_clause(nontrivial);

            // Literals to recover a readable witness if this t is SAT.
            std::vector<CMSat::Lit> e1_x(n_), e1_z(n_), e2_x(n_), e2_z(n_);
            for (std::size_t i = 0; i < n_; ++i) {
                e1_x[i] = g.term_lit(copy_data[0][i].x);
                e1_z[i] = g.term_lit(copy_data[0][i].z);
                e2_x[i] = g.term_lit(copy_data[1][i].x);
                e2_z[i] = g.term_lit(copy_data[1][i].z);
            }

            if (solver.solve() != CMSat::l_True) continue;
            const auto& model = solver.get_model();
            const auto pauli_letter = [&](CMSat::Lit x, CMSat::Lit z) {
                const bool xv = model.at(x.var()) == CMSat::l_True;
                const bool zv = model.at(z.var()) == CMSat::l_True;
                if (xv && zv) return 'Y';
                if (xv) return 'X';
                if (zv) return 'Z';
                return 'I';
            };
            std::string e1_str(n_, 'I'), e2_str(n_, 'I');
            for (std::size_t i = 0; i < n_; ++i) {
                e1_str[i] = pauli_letter(e1_x[i], e1_z[i]);
                e2_str[i] = pauli_letter(e2_x[i], e2_z[i]);
            }

            std::ostringstream detail;
            detail << e1_str << " and " << e2_str << " share the same measurement record at t=" << t
                   << " but their product is a nontrivial logical operator";
            return Failure{t, detail.str()};
        }
        return std::nullopt;
    }

    std::string describe(StateId id) const override {
        std::ostringstream out;
        out << states_[id].fixed_outcomes.size() << " fixed bit(s)";
        return out.str();
    }

    std::string statistics() const override {
        std::ostringstream out;
        out << "sat backend     : " << step_calls_ << " step(s), " << check_calls_ << " check(s), "
            << states_.size() << " state(s)";
        return out.str();
    }

private:
    const QasmProgram& program_for(const CircuitRef& circuit) {
        const std::string key = circuit.qasm.string();
        if (const auto found = programs_.find(key); found != programs_.end()) return found->second;
        return programs_.emplace(key, parse_qasm_file(circuit.qasm)).first->second;
    }

    fpdl::CodeSpec code_;
    sat::StabilizerCode stabilizer_;
    int tau_ = 0;
    std::size_t n_ = 0;
    std::vector<SatState> states_;
    std::size_t next_tag_ = 0;
    std::size_t step_calls_ = 0;
    std::size_t check_calls_ = 0;
    std::map<std::string, QasmProgram> programs_;
};

std::unique_ptr<Backend> make_sat_backend() { return std::make_unique<SatBackend>(); }

} // namespace ftec
