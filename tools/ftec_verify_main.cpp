#include "dd_backend.hpp"
#include "ftec/dag.hpp"
#include "ftec/verify.hpp"

#include <sys/resource.h>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <optional>
#include <string>
#include <vector>

namespace {

// Peak resident set of this process. getrusage reports ru_maxrss in bytes on
// macOS and in kilobytes everywhere else, which is a difference that silently
// makes the number wrong by 1024 if it is not handled.
std::size_t peak_memory_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::size_t>(usage.ru_maxrss);
#else
    return static_cast<std::size_t>(usage.ru_maxrss) * 1024;
#endif
}

std::string human_bytes(std::size_t bytes) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (bytes >= (1u << 30)) out << static_cast<double>(bytes) / (1u << 30) << " GiB";
    else if (bytes >= (1u << 20)) out << static_cast<double>(bytes) / (1u << 20) << " MiB";
    else out << static_cast<double>(bytes) / (1u << 10) << " KiB";
    return out.str();
}

std::string human_seconds(std::chrono::steady_clock::duration elapsed) {
    const double seconds = std::chrono::duration<double>(elapsed).count();
    std::ostringstream out;
    out << std::fixed << std::setprecision(seconds < 10 ? 2 : 1) << seconds << " s";
    return out.str();
}

void usage(const char* argv0) {
    std::cerr
        << "usage: " << argv0 << " <protocol.fpdl> [options]\n"
        << "  --backend=NAME   mock (default) -- see below\n"
        << "  --bound=N        fix the BMC bound instead of searching for one\n"
        << "  --max-paths=N    give up past this many symbolic paths (default 5000)\n"
        << "  --first          stop at the first unprotected path\n"
        << "  --dag            print the merged path structure and exit\n"
        << "\n"
        << "Backends:\n"
        << "  mock   runs the traversal without modelling errors: every branch is\n"
        << "         taken and nothing ever fails. Useful for inspecting a protocol's\n"
        << "         structure, not for deciding whether it is fault tolerant.\n"
        << "  dd     decision diagrams over Pauli sets: propagates the error set\n"
        << "         through each circuit, injects every two-qubit fault, and asks\n"
        << "         whether any reachable set holds two errors whose product is a\n"
        << "         logical operator.\n";
}

// ---------------------------------------------------------------------------
// A backend that models nothing physical, only the *shape* of one.
//
// A real backend returns the outcomes a circuit can actually produce, and that
// set is small because the fault budget is small: with tau faults only so many
// measurements can deviate from what a fault-free run would report. This one
// mimics exactly that constraint and nothing else -- it starts from the
// all-zero outcome and will report a deviating one only while fewer than tau
// deviations have happened along the path.
//
// The budget is not decoration. Without it the mock claims every register
// value at every step, which is 4^depth records for a protocol like CB18 and
// says nothing about whether the driver is right; with it, the walk is bounded
// the way a real one is, and both sides of every guard still get exercised.
// ---------------------------------------------------------------------------
class MockBackend : public ftec::Backend {
public:
    void begin(const fpdl::CodeSpec&, int tau) override {
        tau_ = tau;
        used_.assign(1, 0);
        steps_.clear();
    }

    StateId initial_state() override { return 0; }

    std::vector<std::pair<ftec::Outcome, StateId>> step(StateId id,
                                                        const ftec::CircuitRef& circuit) override {
        steps_.push_back(circuit.se_name);
        const bool flagged = circuit.flag_qubits.has_value();

        std::vector<std::pair<ftec::Outcome, StateId>> out;
        const auto emit = [&](bool s, bool f, int cost) {
            if (used_[id] + cost > tau_) return;
            ftec::Outcome outcome;
            outcome.syndrome = {s};
            if (flagged) outcome.flag = {f};
            used_.push_back(used_[id] + cost);
            out.emplace_back(std::move(outcome), used_.size() - 1);
        };
        emit(false, false, 0);
        emit(true, false, 1);
        if (flagged) {
            emit(false, true, 1);
            emit(true, true, 1);
        }
        return out;
    }

    std::optional<ftec::Failure> check(StateId) override { return std::nullopt; }

    const std::vector<std::string>& steps() const { return steps_; }

private:
    int                      tau_ = 0;
    std::vector<int>         used_;
    std::vector<std::string> steps_;
};

// Symbolic expansion needs a finite transition bound, and a protocol that has
// not finished expanding is silently incomplete. Rather than make the caller
// guess, raise the bound until the parser stops reporting truncation.
struct Expansion {
    fpdl::ParseResult result;
    std::size_t       bound = 0;
};

Expansion expand(const std::filesystem::path& source, std::optional<std::size_t> fixed,
                 std::size_t max_paths) {
    fpdl::ParseOptions options;
    options.max_paths = max_paths;

    if (fixed) {
        options.bmc_bound = *fixed;
        return {fpdl::Parser::parse_file(source, options), *fixed};
    }

    // Doubling keeps the number of attempts logarithmic; the cap is a safety
    // net for a protocol that genuinely never terminates.
    static constexpr std::size_t kCeiling = 1u << 16;
    for (std::size_t bound = 16; bound <= kCeiling; bound *= 2) {
        options.bmc_bound = bound;
        auto parsed = fpdl::Parser::parse_file(source, options);
        const bool bounded_out = parsed.truncated;
        bool any_path_bounded = false;
        for (const auto& path : parsed.paths) any_path_bounded |= path.bound_exceeded;
        if (!bounded_out && !any_path_bounded) return {std::move(parsed), bound};
    }
    throw std::runtime_error(
        "symbolic expansion did not converge below a bound of " + std::to_string(kCeiling) +
        "; pass --bound=N to choose one explicitly");
}

void print_dag(const ftec::Dag& dag) {
    std::map<ftec::DagNode::Kind, std::size_t> kinds;
    for (const auto& node : dag.nodes) ++kinds[node.kind];

    std::cout << "protocol   : " << dag.protocol << "\n"
              << "code       : [[" << dag.code.n << ',' << dag.code.k << ',' << dag.code.d
              << "]], tau = " << dag.code.fault_budget() << "\n"
              << "paths      : " << dag.path_count << "\n"
              << "nodes      : " << dag.nodes.size() << " ("
              << kinds[ftec::DagNode::Kind::Se] << " SE, "
              << kinds[ftec::DagNode::Kind::Terminal] << " terminal)\n"
              << "edges      : " << dag.edges.size() << "\n\n";

    for (const auto& node : dag.nodes) {
        if (node.kind != ftec::DagNode::Kind::Se) continue;
        std::cout << "  #" << node.id << "  round " << node.round << '.' << node.invocation
                  << ' ' << node.phase << "  " << node.circuit.se_name << "  ("
                  << node.circuit.qasm.filename().string() << ")\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::filesystem::path      source;
    std::string                backend_name = "mock";
    std::optional<std::size_t> bound;
    std::size_t                max_paths = 5000;
    bool                       dag_only  = false;
    ftec::VerifyOptions        options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--backend=", 0) == 0) {
            backend_name = arg.substr(10);
        } else if (arg.rfind("--bound=", 0) == 0) {
            bound = std::stoull(arg.substr(8));
        } else if (arg.rfind("--max-paths=", 0) == 0) {
            max_paths = std::stoull(arg.substr(12));
        } else if (arg == "--first") {
            options.stop_at_first_failure = true;
        } else if (arg == "--dag") {
            dag_only = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "error: unknown option " << arg << "\n";
            usage(argv[0]);
            return 2;
        } else if (source.empty()) {
            source = arg;
        } else {
            std::cerr << "error: more than one protocol given\n";
            return 2;
        }
    }
    if (source.empty()) {
        usage(argv[0]);
        return 2;
    }

    const auto started = std::chrono::steady_clock::now();
    ftec::BackendPtr backend;
    try {
        const auto expansion = expand(source, bound, max_paths);
        const auto dag = ftec::build_dag(expansion.result, source.parent_path());
        const auto expanded_at = std::chrono::steady_clock::now();

        // Printed at the end of every route out of here, including the early
        // ones, so a run that was abandoned still says what it cost.
        const auto report_cost = [&](std::chrono::steady_clock::duration traversal) {
            if (backend) {
                const std::string extra = backend->statistics();
                if (!extra.empty()) std::cout << "\n" << extra << "\n";
            }
            std::cout << "\nexpansion       : " << human_seconds(expanded_at - started) << "\n"
                      << "traversal       : " << human_seconds(traversal) << "\n"
                      << "total runtime   : "
                      << human_seconds(std::chrono::steady_clock::now() - started) << "\n"
                      << "peak memory     : " << human_bytes(peak_memory_bytes()) << "\n";
        };

        if (dag_only) {
            print_dag(dag);
            std::cout << "\nBMC bound  : " << expansion.bound
                      << (bound ? " (given)" : " (found by doubling)") << "\n";
            report_cost(std::chrono::steady_clock::duration::zero());
            return 0;
        }

        if (backend_name == "dd") {
            backend = ftec::make_dd_backend();
        } else if (backend_name == "mock") {
            backend = std::make_unique<MockBackend>();
        } else {
            std::cerr << "error: unknown backend '" << backend_name << "'\n";
            usage(argv[0]);
            return 2;
        }

        const auto traversal_started = std::chrono::steady_clock::now();
        const auto result = ftec::verify(dag, *backend, options);
        const auto traversal = std::chrono::steady_clock::now() - traversal_started;

        std::cout << "protocol        : " << result.protocol << "\n"
                  << "code            : [[" << dag.code.n << ',' << dag.code.k << ','
                  << dag.code.d << "]], tau = " << result.tau << "\n"
                  << "BMC bound       : " << expansion.bound
                  << (bound ? " (given)" : " (found by doubling)") << "\n"
                  << "backend         : " << backend_name << "\n"
                  << "paths reached   : " << result.paths_reached << " of " << dag.path_count
                  << "\n"
                  << "records reached : " << result.records_reached << "\n"
                  << "circuits run    : " << result.se_applications << "\n\n";

        if (result.clean()) {
            std::cout << "no unprotected path found";
            if (backend_name == "mock") {
                std::cout << " -- but the mock backend never reports one, so this says\n"
                             "nothing about fault tolerance; it only means the traversal "
                             "completed";
            }
            std::cout << "\n";
            report_cost(traversal);
            return 0;
        }

        std::cout << result.failures.size() << " unprotected path(s); first failure at t = "
                  << result.min_fault_count << "\n\n";
        for (const auto& failure : result.failures) {
            std::cout << "  path " << failure.path_id << "  t=" << failure.failure.fault_count
                      << "  record=" << failure.record_string() << "\n";
            if (!failure.failure.detail.empty()) {
                std::cout << "        " << failure.failure.detail << "\n";
            }
        }
        report_cost(traversal);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
