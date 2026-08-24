#include "stabilizer_gf2.hpp"

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

// Cross-checks against the literature: CR17's [[5,1,3]] generators
// (XZZXI, IXZZX, XIXZZ, ZXIXZ) have well-known logical X/Z = XXXXX/ZZZZZ.
void test_cr17_logical_operators() {
    fpdl::CodeSpec code;
    code.n = 5;
    code.k = 1;
    code.generators = {"XZZXI", "IXZZX", "XIXZZ", "ZXIXZ"};

    const auto sc = sat::build_stabilizer_code(code);
    check(sc.generators.size() == 4, "4 generators parsed");
    check(sc.logical_operators.size() == 2, "2k=2 logical operators computed");
    if (sc.logical_operators.size() != 2) return;

    const auto& x_bar = sc.logical_operators[0];
    const auto& z_bar = sc.logical_operators[1];
    for (const auto& g : sc.generators) {
        check(!sat::symplectic_product(x_bar, g), "logical X commutes with every generator");
        check(!sat::symplectic_product(z_bar, g), "logical Z commutes with every generator");
    }
    check(sat::symplectic_product(x_bar, z_bar), "logical X anticommutes with logical Z");

    const auto xxxxx = sat::parse_pauli_string("XXXXX");
    const auto zzzzz = sat::parse_pauli_string("ZZZZZ");
    // Stabilizer-equivalent to the literature's X_bar/Z_bar means their
    // product with the known operator is itself in the normalizer with
    // *trivial* logical action, i.e. it's a stabilizer element.
    const auto diff_x = sat::multiply(x_bar, xxxxx);
    const auto diff_z = sat::multiply(z_bar, zzzzz);
    check(sat::in_normalizer(diff_x, sc) && !sat::has_nontrivial_logical_action(diff_x, sc),
          "computed logical X is stabilizer-equivalent to XXXXX");
    check(sat::in_normalizer(diff_z, sc) && !sat::has_nontrivial_logical_action(diff_z, sc),
          "computed logical Z is stabilizer-equivalent to ZZZZZ");

    check(!sat::in_normalizer(sat::parse_pauli_string("XIIII"), sc),
          "a lone X on qubit 1 is not in N(S) (anticommutes with ZXIXZ)");
    for (const auto& g : sc.generators)
        check(!sat::has_nontrivial_logical_action(g, sc), "a stabilizer generator has trivial logical action");
}

} // namespace

int main() {
    test_cr17_logical_operators();
    if (failures > 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
