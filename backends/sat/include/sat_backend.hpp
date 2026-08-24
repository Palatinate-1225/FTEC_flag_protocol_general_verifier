#pragma once

#include "ftec/backend.hpp"

#include <memory>

namespace ftec {

// Symbolic Pauli propagation (backends/sat/pauli_gf2) plus a CryptoMiniSat
// encoding of the paper's SAT queries, behind the same ftec::Backend
// interface the dd backend implements. See README.md for current status:
// step() is real; check() is not yet (always reports no failure).
std::unique_ptr<Backend> make_sat_backend();

} // namespace ftec
