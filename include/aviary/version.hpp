#pragma once

#include <stdexcept>
#include <string>

namespace aviary {

/// Initializes the cryptographic backend (cipherbird). Must be called once
/// before any crypto operation. Throws std::runtime_error on failure.
void init();

/// Aviary library version (semver string).
std::string version();

/// Version reported by the linked cipherbird crypto backend. A non-empty return
/// proves the C++ core -> cipherbird_c link is live end to end.
std::string crypto_backend_version();

}  // namespace aviary
