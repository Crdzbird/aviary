// Zero-dependency smoke test (matching cryptolib's convention).
// Proves the C++ core links cryptolib_c and computes BLAKE3 correctly.
#include <cstdio>
#include <string>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/version.hpp"

namespace {
int g_failures = 0;

void check(bool cond, const char* what) {
  if (cond) {
    std::printf("  ok   %s\n", what);
  } else {
    std::printf("  FAIL %s\n", what);
    ++g_failures;
  }
}

std::vector<std::uint8_t> bytes(const char* s) {
  return std::vector<std::uint8_t>(s, s + std::char_traits<char>::length(s));
}
}  // namespace

int main() {
  using namespace aviary;
  init();

  check(!version().empty(), "aviary::version() non-empty");
  check(!crypto_backend_version().empty(),
        "crypto_backend_version() non-empty (cryptolib linkage live)");
  std::printf("  ..   backend = cryptolib %s\n", crypto_backend_version().c_str());

  // Externally-anchored golden vector: the official BLAKE3 hash of the empty
  // input. If this matches, blake3 routed through cryptolib_c is correct.
  const auto h_empty = crypto::blake3_256(std::vector<std::uint8_t>{});
  check(crypto::to_hex(h_empty) ==
            "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262",
        "BLAKE3('') matches official vector");

  // Structural + determinism properties.
  const auto h1 = crypto::blake3_256(bytes("aviary"));
  const auto h2 = crypto::blake3_256(bytes("aviary"));
  check(h1.size() == 32, "digest width is 32 bytes");
  check(h1 == h2, "hashing is deterministic");
  check(h1 != h_empty, "distinct inputs hash distinctly");
  std::printf("  ..   blake3('aviary') = %s\n", crypto::to_hex(h1).c_str());

  std::printf(g_failures == 0 ? "\nALL PASS\n" : "\n%d FAILURE(S)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
