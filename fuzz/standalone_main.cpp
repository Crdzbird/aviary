// A file driver for the fuzz targets on toolchains without libFuzzer (Apple
// clang): runs LLVMFuzzerTestOneInput over every file given on the command
// line, plus the empty input, so a target can be smoke tested anywhere.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main(int argc, char** argv) {
  const std::uint8_t empty = 0;
  LLVMFuzzerTestOneInput(&empty, 0);
  for (int i = 1; i < argc; ++i) {
    std::ifstream in(argv[i], std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    std::printf("ok %s (%zu bytes)\n", argv[i], bytes.size());
  }
  return 0;
}
