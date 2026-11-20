#include "aviary/version.hpp"

#include <stdexcept>

#include "cipherbird_c.h"

namespace aviary {

namespace {
#ifndef AVIARY_VERSION_STRING
#define AVIARY_VERSION_STRING "0.0.0"
#endif
#ifndef AVIARY_CIPHERBIRD_REVISION
#define AVIARY_CIPHERBIRD_REVISION "unknown"
#endif
constexpr const char* kVersion = AVIARY_VERSION_STRING;
constexpr const char* kCipherBirdRevision = AVIARY_CIPHERBIRD_REVISION;
}

void init() {
  if (cipherbird_init() != 0) {
    throw std::runtime_error("aviary::init: cipherbird_init failed");
  }
}

std::string version() { return kVersion; }

std::string crypto_backend_version() {
  const char* v = cipherbird_version();  // static string, do not free
  std::string out = v != nullptr ? std::string(v) : std::string();
  return out + " (CipherBird " + kCipherBirdRevision + ")";
}

}  // namespace aviary
