// Embedded log-structured KV store: append-only log + in-memory offset index,
// durable across reopen, with tombstone deletes and space-reclaiming compaction.
#include <filesystem>
#include <string>

#include "aviary/kvstore.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using storage::LogKvStore;

namespace {
Bytes b(const char* s) {
  Bytes out;
  for (; *s; ++s) out.push_back(static_cast<std::uint8_t>(*s));
  return out;
}
codec::ByteView view(const Bytes& x) { return codec::ByteView(x.data(), x.size()); }
}  // namespace

int main() {
  init();
  const std::filesystem::path path = std::filesystem::temp_directory_path() / "aviary_kv_test" / "log";
  std::filesystem::remove_all(path.parent_path());

  std::printf("== put / get / has / overwrite / erase ==\n");
  {
    LogKvStore kv(path);
    kv.put("alice", view(b("100")));
    kv.put("bob", view(b("42")));
    CHECK(kv.has("alice") && kv.has("bob"));
    CHECK(*kv.get("alice") == b("100"));
    CHECK(!kv.get("carol").has_value());
    CHECK(kv.size() == 2);

    // Overwrite: last write wins.
    kv.put("alice", view(b("900")));
    CHECK(*kv.get("alice") == b("900"));
    CHECK(kv.size() == 2);

    // Erase (tombstone).
    kv.erase("bob");
    CHECK(!kv.has("bob"));
    CHECK(kv.size() == 1);
    CHECK(!kv.get("bob").has_value());
  }

  std::printf("== durability: reopen replays the log, index rebuilt ==\n");
  {
    LogKvStore kv(path);  // reopen the same file
    CHECK(kv.size() == 1);
    CHECK(*kv.get("alice") == b("900"));  // the overwrite survived
    CHECK(!kv.has("bob"));                // the delete survived
    CHECK(!kv.get("bob").has_value());
    // Add more so there is dead space to reclaim.
    kv.put("carol", view(b("7")));
    kv.put("carol", view(b("77")));
    kv.put("carol", view(b("777")));  // two dead versions of carol
    kv.erase("alice");                // alice now dead too
    CHECK(kv.size() == 1);            // only carol is live
    CHECK(*kv.get("carol") == b("777"));
  }

  std::printf("== compaction reclaims dead space, preserves live data ==\n");
  {
    LogKvStore kv(path);
    const std::uint64_t before = kv.log_bytes();
    CHECK(before > 0);
    CHECK(kv.size() == 1);  // carol only

    const std::uint64_t reclaimed = kv.compact();
    CHECK(reclaimed > 0);                 // dead records were dropped
    CHECK(kv.log_bytes() < before);       // log shrank
    CHECK(kv.size() == 1);                // live set unchanged
    CHECK(*kv.get("carol") == b("777"));  // live value intact

    // Compaction persists: reopen sees only the compacted log.
  }
  {
    LogKvStore kv(path);
    CHECK(kv.size() == 1);
    CHECK(*kv.get("carol") == b("777"));
    CHECK(!kv.has("alice") && !kv.has("bob"));
  }

  std::printf("== empty value + binary keys round-trip ==\n");
  {
    std::filesystem::remove_all(path.parent_path());
    LogKvStore kv(path);
    kv.put("empty", {});                        // zero-length value
    kv.put(std::string("k\0e\1y", 5), view(b("v")));  // key with NUL + control byte
    CHECK(kv.get("empty").has_value() && kv.get("empty")->empty());
    CHECK(*kv.get(std::string("k\0e\1y", 5)) == b("v"));
  }

  std::filesystem::remove_all(path.parent_path());
  return bctest::summary();
}
