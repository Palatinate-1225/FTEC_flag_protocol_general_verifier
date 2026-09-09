// The dd and spbdd backends answer the same question with different
// machinery, so anything they disagree about is a bug in one of them. This
// runs both over whichever protocols it is given and compares everything the
// two are obliged to share.
//
// What is *not* compared is Failure::detail: it names a witness pair, and a
// set can hold several, so which one comes back is the library's business. The
// fault count it comes back with is not -- that is the answer.

#include "dd_backend.hpp"
#include "ftec/dag.hpp"
#include "ftec/verify.hpp"
#include "spbdd_backend.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (condition) return;
    std::cerr << "  FAILED: " << what << '\n';
    ++failures;
}

// The same doubling the CLI does: a protocol that has not finished expanding
// is silently incomplete, so raise the bound until the parser stops truncating.
fpdl::ParseResult expand(const std::filesystem::path& source) {
    fpdl::ParseOptions options;
    options.max_paths = 5000;
    for (std::size_t bound = 16; bound <= (1u << 16); bound *= 2) {
        options.bmc_bound = bound;
        auto parsed = fpdl::Parser::parse_file(source, options);
        bool any_path_bounded = false;
        for (const auto& path : parsed.paths) any_path_bounded |= path.bound_exceeded;
        if (!parsed.truncated && !any_path_bounded) return parsed;
    }
    throw std::runtime_error("symbolic expansion did not converge");
}

// Everything the two backends have to agree on, as one comparable string.
std::string summary(const ftec::VerifyResult& result) {
    std::ostringstream out;
    out << "paths=" << result.paths_reached << " records=" << result.records_reached
        << " circuits=" << result.se_applications << " min_t=" << result.min_fault_count
        << " failures=" << result.failures.size();
    for (const auto& failure : result.failures) {
        out << "\n    path " << failure.path_id << " t=" << failure.failure.fault_count
            << " record=" << failure.record_string();
    }
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <protocol.fpdl>...\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        const std::filesystem::path source = argv[i];
        std::cout << source.filename().string() << '\n';

        try {
            const auto parsed = expand(source);
            const auto dag    = ftec::build_dag(parsed, source.parent_path());

            // One at a time: BuDDy is a process-wide singleton, so the dd
            // backend has to be gone before anything else claims it. Nothing
            // stops the spbdd one from overlapping, but symmetry is cheaper
            // than remembering which is which.
            std::string dd_summary;
            {
                auto backend = ftec::make_dd_backend();
                dd_summary   = summary(ftec::verify(dag, *backend));
            }
            std::string spbdd_summary;
            {
                auto backend  = ftec::make_spbdd_backend();
                spbdd_summary = summary(ftec::verify(dag, *backend));
            }

            check(dd_summary == spbdd_summary,
                  source.filename().string() + ": the backends disagree\n    dd   : " +
                      dd_summary + "\n    spbdd: " + spbdd_summary);
            if (dd_summary == spbdd_summary) std::cout << "  agree: " << dd_summary << '\n';
        } catch (const std::exception& error) {
            check(false, source.filename().string() + ": " + error.what());
        }
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all backends agree\n";
    return 0;
}
