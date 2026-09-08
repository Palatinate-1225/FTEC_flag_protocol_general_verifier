#pragma once

#include "ftec/backend.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ftec {

// How the backend should have CUDD reorder its variables.
//
// SPBDD's own ManagerConfig offers one bit -- reordering on or off -- and
// hard-codes the method to sifting. CUDD has a dozen more and a threshold that
// decides when a reordering fires, and on this workload the difference between
// them is larger than the difference between the two BDD packages, so the
// backend reaches past SPBDD to Manager::raw() and sets them itself.
struct SpbddReorder {
    // One of the names in kSpbddReorderMethods below. "none" disables
    // reordering, which is SPBDD's own default; "sift" is what SPBDD asks for
    // when told to reorder, and what the dd backend asks BuDDy for.
    std::string method = "sift";

    // Cudd_SetNextReordering: the live-node count at which the *first*
    // reordering fires. 0 leaves CUDD's own default (4004) alone. Later
    // thresholds follow CUDD's own rule -- roughly twice the size the diagram
    // had after the previous reordering -- so this moves the whole schedule
    // without pinning it.
    int threshold = 0;
};

// Every method this backend accepts, roughly cheapest first. Both of these
// read one table in the .cpp, so there is no second list to keep in step.
[[nodiscard]] std::vector<std::string> spbdd_reorder_methods();
[[nodiscard]] bool is_spbdd_reorder_method(const std::string& name);

// The same error model as the dd backend, over the SPBDD library instead of
// BuDDy: Pauli sets as decision diagrams, a state being the family of sets
// indexed by fault count, a two-qubit gate spawning level t into level t+1,
// and a measurement splitting the state on the outcome it can produce.
//
// See dd_backend.hpp for the model itself -- it is deliberately unchanged, so
// the two backends can be run against each other and disagreeing is a bug.
// What differs is the machinery underneath:
//
//   * CUDD rather than BuDDy, so the package is an object rather than a
//     process-wide singleton and several backends may be alive at once.
//   * The set operations this backend needs -- fault injection, reset, the
//     measurement split, the N(S)\S query -- are already the library's own
//     vocabulary, so the layer that used to spell them out in terms of
//     quantification and a symplectic change of basis is gone.
//
// The default asks for sifting, matching what the dd backend asks BuDDy for.
// That is the comparable setting, not the fast one: see the README.
std::unique_ptr<Backend> make_spbdd_backend(const SpbddReorder& reorder = {});

} // namespace ftec
