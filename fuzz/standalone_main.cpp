// A file driver for the fuzz targets on toolchains without libFuzzer (Apple
// clang): runs LLVMFuzzerTestOneInput over every file given on the command
// line, plus the empty input, so a target can be smoke tested anywhere.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {
std::uint64_t next(std::uint64_t& s) {
  s ^= s << 13;
  s ^= s >> 7;
  s ^= s << 17;
  return s;
}

std::vector<std::uint8_t> mutate(const std::vector<std::uint8_t>& in, std::uint64_t& rng) {
  std::vector<std::uint8_t> b = in;
  const int edits = 1 + static_cast<int>(next(rng) % 4);
  for (int e = 0; e < edits; ++e) {
    const std::size_t n = b.size();
    switch (next(rng) % 5) {
      case 0:
        if (n) b[next(rng) % n] ^= static_cast<std::uint8_t>(1u << (next(rng) % 8));
        break;
      case 1:
        if (n) b[next(rng) % n] = static_cast<std::uint8_t>(next(rng));
        break;
      case 2:
        if (n) b.resize(next(rng) % n);
        break;
      case 3:
        if (n) {
          const std::size_t at = next(rng) % n, len = 1 + next(rng) % 16;
          std::vector<std::uint8_t> chunk(b.begin() + at, b.begin() + std::min(n, at + len));
          b.insert(b.begin() + next(rng) % (n + 1), chunk.begin(), chunk.end());
        }
        break;
      default:
        for (int k = 0; k < 4; ++k) b.insert(b.begin() + (b.empty() ? 0 : next(rng) % b.size()), static_cast<std::uint8_t>(next(rng)));
    }
  }
  return b;
}
}

int main(int argc, char** argv) {
  const std::uint8_t empty = 0;
  LLVMFuzzerTestOneInput(&empty, 0);
  for (int i = 1; i < argc; ++i) {
    std::ifstream in(argv[i], std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    const char* env = std::getenv("AVIARY_FUZZ_MUTATIONS");
    const long rounds = env ? std::strtol(env, nullptr, 10) : 0;
    std::uint64_t rng = 0x9E3779B97F4A7C15ull ^ static_cast<std::uint64_t>(i);
    for (long r = 0; r < rounds; ++r) {
      const std::vector<std::uint8_t> m = mutate(bytes, rng);
      LLVMFuzzerTestOneInput(m.data(), m.size());
    }
    std::printf("ok %s (%zu bytes, %ld mutations)\n", argv[i], bytes.size(), rounds);
  }
  return 0;
}
