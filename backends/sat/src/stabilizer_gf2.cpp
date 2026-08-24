#include "stabilizer_gf2.hpp"

#include <algorithm>

namespace sat {

PauliString parse_pauli_string(const std::string& letters) {
    PauliString p(letters.size());
    for (std::size_t i = 0; i < letters.size(); ++i) {
        switch (letters[i]) {
            case 'I': p[i] = {false, false}; break;
            case 'X': p[i] = {true, false}; break;
            case 'Y': p[i] = {true, true}; break;
            case 'Z': p[i] = {false, true}; break;
            default: throw StabilizerError("invalid Pauli letter '" + std::string(1, letters[i]) + "'");
        }
    }
    return p;
}

bool symplectic_product(const PauliString& a, const PauliString& b) {
    bool result = false;
    for (std::size_t i = 0; i < a.size(); ++i)
        result = result != ((a[i].x && b[i].z) != (a[i].z && b[i].x));
    return result;
}

PauliString multiply(const PauliString& a, const PauliString& b) {
    PauliString result(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i].x = a[i].x != b[i].x;
        result[i].z = a[i].z != b[i].z;
    }
    return result;
}

bool in_normalizer(const PauliString& q, const StabilizerCode& code) {
    for (const auto& g : code.generators)
        if (symplectic_product(q, g)) return false;
    return true;
}

bool has_nontrivial_logical_action(const PauliString& q, const StabilizerCode& code) {
    for (const auto& l : code.logical_operators)
        if (symplectic_product(q, l)) return true;
    return false;
}

namespace {

// A 2n-bit vector: [0,n) is the X-part, [n,2n) is the Z-part.
using Bits = std::vector<bool>;

Bits to_bits(const PauliString& p) {
    const std::size_t n = p.size();
    Bits b(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        b[i] = p[i].x;
        b[n + i] = p[i].z;
    }
    return b;
}

PauliString from_bits(const Bits& b) {
    const std::size_t n = b.size() / 2;
    PauliString p(n);
    for (std::size_t i = 0; i < n; ++i) {
        p[i].x = b[i];
        p[i].z = b[n + i];
    }
    return p;
}

// [z-part, x-part]: dot(swapped(a), b) == symplectic_product(a,b) under the
// ordinary (x-part,z-part) encoding of b.
Bits swapped_bits(const PauliString& p) {
    const std::size_t n = p.size();
    Bits b(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        b[i] = p[i].z;
        b[n + i] = p[i].x;
    }
    return b;
}

Bits xor_bits(const Bits& a, const Bits& b) {
    Bits r(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) r[i] = a[i] != b[i];
    return r;
}

bool symplectic_product_bits(const Bits& a, const Bits& b) {
    const std::size_t n = a.size() / 2;
    bool result = false;
    for (std::size_t i = 0; i < n; ++i)
        result = result != ((a[i] && b[n + i]) != (a[n + i] && b[i]));
    return result;
}

// Incrementally-built GF(2) row-echelon basis, used only to test/track
// ordinary linear independence (not the symplectic form).
class LinearBasis {
public:
    Bits reduce(Bits v) const {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (v[pivot_col_[i]]) v = xor_bits(v, rows_[i]);
        }
        return v;
    }

    bool try_add(const Bits& v) {
        const Bits r = reduce(v);
        for (std::size_t c = 0; c < r.size(); ++c) {
            if (r[c]) {
                rows_.push_back(r);
                pivot_col_.push_back(c);
                return true;
            }
        }
        return false;
    }

private:
    std::vector<Bits> rows_;
    std::vector<std::size_t> pivot_col_;
};

// Null space of {v : dot(rows[i], v) == 0 for all i}, over GF(2).
std::vector<Bits> null_space(std::vector<Bits> rows, std::size_t num_cols) {
    std::vector<std::size_t> pivot_cols;
    std::size_t rank = 0;
    for (std::size_t col = 0; col < num_cols && rank < rows.size(); ++col) {
        std::size_t sel = rank;
        while (sel < rows.size() && !rows[sel][col]) ++sel;
        if (sel == rows.size()) continue;
        std::swap(rows[rank], rows[sel]);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            if (r != rank && rows[r][col]) rows[r] = xor_bits(rows[r], rows[rank]);
        }
        pivot_cols.push_back(col);
        ++rank;
    }
    rows.resize(rank);

    std::vector<bool> is_pivot_col(num_cols, false);
    for (auto c : pivot_cols) is_pivot_col[c] = true;

    std::vector<Bits> basis;
    for (std::size_t free_col = 0; free_col < num_cols; ++free_col) {
        if (is_pivot_col[free_col]) continue;
        Bits v(num_cols, false);
        v[free_col] = true;
        for (std::size_t r = 0; r < rank; ++r)
            if (rows[r][free_col]) v[pivot_cols[r]] = true;
        basis.push_back(std::move(v));
    }
    return basis;
}

// Extends the (mutually commuting, independent) stabilizer generators to a
// full symplectic basis, returning the 2k logical operators X1,Z1,...,Xk,Zk.
std::vector<PauliString> compute_logical_operators(const std::vector<PauliString>& generators,
                                                     std::size_t n, std::size_t k) {
    if (k == 0) return {};

    std::vector<Bits> gen_bits;
    for (const auto& g : generators) gen_bits.push_back(to_bits(g));

    LinearBasis used;
    for (const auto& gb : gen_bits) {
        if (!used.try_add(gb)) throw StabilizerError("stabilizer generators are not linearly independent");
    }
    for (std::size_t i = 0; i < gen_bits.size(); ++i)
        for (std::size_t j = i + 1; j < gen_bits.size(); ++j)
            if (symplectic_product_bits(gen_bits[i], gen_bits[j]))
                throw StabilizerError("stabilizer generators do not mutually commute");

    std::vector<Bits> swapped_rows;
    for (const auto& g : generators) swapped_rows.push_back(swapped_bits(g));
    const std::vector<Bits> normalizer_basis = null_space(swapped_rows, 2 * n);

    std::vector<Bits> x_logicals, z_logicals;
    for (std::size_t i = 0; i < k; ++i) {
        Bits x_i;
        bool found = false;
        for (const auto& candidate : normalizer_basis) {
            if (used.try_add(candidate)) {
                x_i = candidate;
                found = true;
                break;
            }
        }
        if (!found)
            throw StabilizerError("could not find logical operator " + std::to_string(i + 1) +
                                  " (code declaration may be malformed)");

        for (std::size_t j = 0; j < x_logicals.size(); ++j) {
            if (symplectic_product_bits(x_i, x_logicals[j])) x_i = xor_bits(x_i, z_logicals[j]);
            if (symplectic_product_bits(x_i, z_logicals[j])) x_i = xor_bits(x_i, x_logicals[j]);
        }

        Bits z_i;
        bool paired = false;
        for (const auto& candidate : normalizer_basis) {
            if (symplectic_product_bits(x_i, candidate)) {
                z_i = candidate;
                paired = true;
                break;
            }
        }
        if (!paired)
            throw StabilizerError("could not find a symplectic partner for logical operator " +
                                  std::to_string(i + 1));

        for (std::size_t j = 0; j < x_logicals.size(); ++j) {
            if (symplectic_product_bits(z_i, x_logicals[j])) z_i = xor_bits(z_i, z_logicals[j]);
            if (symplectic_product_bits(z_i, z_logicals[j])) z_i = xor_bits(z_i, x_logicals[j]);
        }
        if (!symplectic_product_bits(x_i, z_i))
            throw StabilizerError("internal error: logical-pair fixup broke anticommutation");
        if (!used.try_add(z_i))
            throw StabilizerError("internal error: logical partner was not independent as expected");

        x_logicals.push_back(x_i);
        z_logicals.push_back(z_i);
    }

    std::vector<PauliString> logicals;
    for (std::size_t i = 0; i < k; ++i) {
        logicals.push_back(from_bits(x_logicals[i]));
        logicals.push_back(from_bits(z_logicals[i]));
    }
    return logicals;
}

} // namespace

StabilizerCode build_stabilizer_code(const fpdl::CodeSpec& code) {
    StabilizerCode result;
    result.n = static_cast<std::size_t>(code.n);
    result.k = static_cast<std::size_t>(code.k);
    for (const auto& letters : code.generators) result.generators.push_back(parse_pauli_string(letters));
    result.logical_operators = compute_logical_operators(result.generators, result.n, result.k);
    return result;
}

} // namespace sat
