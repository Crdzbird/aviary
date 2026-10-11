#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/types.hpp"

/// An embedded log-structured key->bytes store (ARCHITECTURE section 13; Bitcask-style),
/// with no external dependency. Writes APPEND a record to a single log file; an
/// in-memory index maps each key to its latest value's offset in the log, so a
/// read is one seek + read. Reopening replays the log to rebuild the index
/// (last write wins; a tombstone deletes). `compact` rewrites the log with only
/// live records, reclaiming the space of overwritten / deleted entries.
///
/// This is the durable engine the whole-file `storage::FileStore` was a
/// placeholder for: values are not all held in memory, and updates are O(1)
/// appends instead of O(state) rewrites.
namespace aviary::storage {

class LogKvStore {
 public:
  /// Open (creating if needed) the store at [path], replaying its log.
  explicit LogKvStore(std::filesystem::path path);

  void put(const std::string& key, codec::ByteView value);
  std::optional<Bytes> get(const std::string& key) const;
  bool has(const std::string& key) const { return index_.count(key) != 0; }
  void erase(const std::string& key);

  std::size_t size() const { return index_.size(); }  // live keys
  std::vector<std::string> keys() const;

  /// On-disk log size in bytes (grows with every write; shrinks on compact).
  std::uint64_t log_bytes() const { return log_size_; }

  /// Rewrite the log keeping only live records. Returns the bytes reclaimed.
  std::uint64_t compact();

 private:
  struct Loc {
    std::uint64_t offset = 0;  // byte offset of the value in the log
    std::uint32_t len = 0;     // value length
  };

  void replay();
  Loc append(const std::string& key, codec::ByteView value, bool tombstone);

  std::filesystem::path path_;
  mutable std::fstream file_;  // append writes + random reads
  std::map<std::string, Loc> index_;
  std::uint64_t log_size_ = 0;
};

}  // namespace aviary::storage
