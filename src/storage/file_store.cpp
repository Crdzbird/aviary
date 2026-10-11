#include <optional>
#include "aviary/storage.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace aviary::storage {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

void FileStore::put(const std::string& key, codec::ByteView bytes) const {
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);  // no-op if it exists
  const std::filesystem::path final = path_for(key);
  const std::filesystem::path tmp = path_for(key + ".tmp");

  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("storage: cannot open temp file for " + key);
    if (!bytes.empty()) {
      out.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    out.flush();
    if (!out) throw std::runtime_error("storage: write failed for " + key);
  }
  // Atomic replace: a crash before this leaves the old value intact.
  std::filesystem::rename(tmp, final, ec);
  if (ec) {
    std::filesystem::remove(tmp);
    throw std::runtime_error("storage: rename failed for " + key + ": " + ec.message());
  }
}

std::optional<Bytes> FileStore::get(const std::string& key) const {
  const std::filesystem::path p = path_for(key);
  std::ifstream in(p, std::ios::binary | std::ios::ate);
  if (!in) return std::nullopt;
  const std::streamsize size = in.tellg();
  if (size < 0) return std::nullopt;
  Bytes out(static_cast<std::size_t>(size));
  in.seekg(0);
  if (size > 0) in.read(reinterpret_cast<char*>(out.data()), size);
  if (!in && size > 0) throw std::runtime_error("storage: read failed for " + key);
  return out;
}

bool FileStore::has(const std::string& key) const {
  std::error_code ec;
  return std::filesystem::exists(path_for(key), ec) && !ec;
}

void FileStore::remove(const std::string& key) const {
  std::error_code ec;
  std::filesystem::remove(path_for(key), ec);
}

// -- Chain persistence ----------------------------------------------------------

void save_chain(const FileStore& store, const BlockStore& blocks, const Ledger& ledger) {
  const Bytes block_bytes = blocks.serialize();
  const Bytes state_bytes = ledger.snapshot();
  store.put(kBlocksKey, view(block_bytes));
  store.put(kStateKey, view(state_bytes));
}

std::optional<LoadedChain> load_chain(const FileStore& store, ChainSpec spec) {
  std::optional<Bytes> state = store.get(kStateKey);
  if (!state) return std::nullopt;  // nothing persisted yet

  LoadedChain out;
  out.ledger = Ledger::restore(spec, view(*state));
  if (std::optional<Bytes> blocks = store.get(kBlocksKey)) {
    out.blocks = BlockStore::load(view(*blocks));
  }
  return out;
}

}  // namespace aviary::storage
