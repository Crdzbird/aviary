#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "check.hpp"
#include "subchain_vectors.hpp"

#ifndef AVIARY_VECTORS_DIR
#error "AVIARY_VECTORS_DIR must point at the vectors directory"
#endif

int main() {
  CHECK(aviary_init() == 0);
  std::ifstream in(std::string(AVIARY_VECTORS_DIR) + "/subchain_v2_vectors.txt");
  CHECK_MSG(in.good(), "vectors/subchain_v2_vectors.txt is readable");
  std::map<std::string, std::string> pinned;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string name, value;
    fields >> name >> value;
    pinned[name] = value;
  }

  const subchain_vectors::Vectors computed = subchain_vectors::compute();
  CHECK_MSG(pinned.size() == computed.size(), "the file pins exactly the values the scenario produces");
  for (const auto& [name, value] : computed) {
    const bool same = pinned.count(name) == 1 && pinned[name] == value;
    if (!same) std::printf("  mismatch: %s\n", name.c_str());
    bctest::check(same, name.c_str());
  }
  CHECK(pinned["child_proof_2_verified"] == pinned["child_record_2"]);
  CHECK(pinned["child_record_3"].substr(24, 2) == "02" && pinned["child_record_2"].substr(24, 2) == "01");
  CHECK(pinned["subchain_root_1"] != pinned["subchain_root_2"] && pinned["subchain_root_2"] != pinned["subchain_root_3"]);
  return bctest::summary();
}
