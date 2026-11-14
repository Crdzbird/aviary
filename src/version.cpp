#include "aviary/version.hpp"

#include <stdexcept>

#include "cryptolib_c.h"

namespace aviary {

namespace {
#ifndef AVIARY_VERSION_STRING
#define AVIARY_VERSION_STRING "0.0.0"
#endif
#ifndef AVIARY_CRYPTOLIB_REVISION
#define AVIARY_CRYPTOLIB_REVISION "unknown"
#endif
constexpr const char* kVersion = AVIARY_VERSION_STRING;
constexpr const char* kCryptolibRevision = AVIARY_CRYPTOLIB_REVISION;
}

void init() {
  if (cryptolib_init() != 0) {
    throw std::runtime_error("aviary::init: cryptolib_init failed");
  }
}

std::string version() { return kVersion; }

std::string crypto_backend_version() {
  const char* v = cryptolib_version();  // static string, do not free
  std::string out = v != nullptr ? std::string(v) : std::string();
  return out + " (CipherBird " + kCryptolibRevision + ")";
}

}  // namespace aviary
