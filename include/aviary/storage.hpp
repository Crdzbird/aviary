#pragma once

#include <memory>
#include <filesystem>
#include <optional>
#include <string>

#include "aviary/block_store.hpp"
#include "aviary/codec.hpp"
#include "aviary/ledger.hpp"

/// On-disk persistence (ARCHITECTURE section 13). A [FileStore] is a minimal, crash-safe
/// key->bytes backend: each key is a file in a directory, and writes are atomic
/// (temp file + rename) so a crash mid-write never corrupts an existing value.
/// The chain helpers layer the existing serialization (BlockStore::serialize,
/// Ledger::snapshot) on top, so a node can persist its whole chain and reload it
/// on restart.
namespace aviary::storage {

class FileStore {
 public:
  /// Persist under [dir] (created on first write if absent).
  explicit FileStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

  /// Atomically write [bytes] under [key] (temp file + rename). Throws on I/O error.
  void put(const std::string& key, codec::ByteView bytes) const;

  /// Read the value for [key], or nullopt if absent.
  std::optional<Bytes> get(const std::string& key) const;

  bool has(const std::string& key) const;
  void remove(const std::string& key) const;

  const std::filesystem::path& dir() const { return dir_; }

 private:
  std::filesystem::path dir_;
  std::filesystem::path path_for(const std::string& key) const { return dir_ / key; }
};

// -- Chain persistence ----------------------------------------------------------

/// Keys used for the block archive and the state snapshot.
inline constexpr const char* kBlocksKey = "blocks";
inline constexpr const char* kStateKey = "state";

/// Persist the block archive + the ledger's state snapshot to [store].
void save_chain(const FileStore& store, const BlockStore& blocks, const Ledger& ledger);

/// A chain reloaded from disk: the block archive and the restored ledger.
struct LoadedChain {
  BlockStore blocks;
  std::unique_ptr<Ledger> ledger;
};

/// Reload a chain persisted with save_chain, under [spec]. Returns nullopt if no
/// state snapshot is present. Throws on a malformed blob.
std::optional<LoadedChain> load_chain(const FileStore& store, ChainSpec spec);

}  // namespace aviary::storage
