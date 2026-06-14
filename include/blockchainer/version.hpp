#pragma once

#include <string>

namespace blockchainer {

/// Initializes the cryptographic backend (cryptolib). Must be called once
/// before any crypto operation. Throws std::runtime_error on failure.
void init();

/// Blockchainer library version (semver string).
std::string version();

/// Version reported by the linked cryptolib crypto backend. A non-empty return
/// proves the C++ core → cryptolib_c link is live end to end.
std::string crypto_backend_version();

}  // namespace blockchainer
