#include "blockchainer/version.hpp"

#include <stdexcept>

#include "cryptolib_c.h"

namespace blockchainer {

namespace {
constexpr const char* kVersion = "0.0.1";
}

void init() {
  if (cryptolib_init() != 0) {
    throw std::runtime_error("blockchainer::init: cryptolib_init failed");
  }
}

std::string version() { return kVersion; }

std::string crypto_backend_version() {
  const char* v = cryptolib_version();  // static string, do not free
  return v != nullptr ? std::string(v) : std::string();
}

}  // namespace blockchainer
