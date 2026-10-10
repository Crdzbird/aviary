#include "aviary/kvstore.hpp"

#include <stdexcept>

namespace aviary::storage {

namespace {
// value_len sentinel meaning "this record deletes the key".
constexpr std::uint32_t kTombstone = 0xFFFFFFFFu;

void put_u32(std::ostream& os, std::uint32_t v) {
  char b[4] = {static_cast<char>(v >> 24), static_cast<char>(v >> 16),
               static_cast<char>(v >> 8), static_cast<char>(v)};
  os.write(b, 4);
}

// Reads a big-endian u32; returns false at clean EOF.
bool read_u32(std::istream& is, std::uint32_t& out) {
  char b[4];
  is.read(b, 4);
  if (is.gcount() != 4) return false;
  out = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[0])) << 24) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[1])) << 16) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[2])) << 8) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[3])));
  return true;
}
}  // namespace

LogKvStore::LogKvStore(std::filesystem::path path) : path_(std::move(path)) {
  if (!std::filesystem::exists(path_)) {
    if (path_.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(path_.parent_path(), ec);
    }
    std::ofstream create(path_, std::ios::binary);  // touch
  }
  file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
  if (!file_) throw std::runtime_error("kvstore: cannot open " + path_.string());
  replay();
}

void LogKvStore::replay() {
  index_.clear();
  file_.clear();
  file_.seekg(0, std::ios::beg);
  std::uint64_t pos = 0;
  std::uint32_t key_len = 0;
  while (read_u32(file_, key_len)) {
    pos += 4;
    std::string key(key_len, '\0');
    file_.read(key.data(), key_len);
    if (file_.gcount() != static_cast<std::streamsize>(key_len)) break;  // truncated tail
    pos += key_len;
    std::uint32_t vlen = 0;
    if (!read_u32(file_, vlen)) break;
    pos += 4;
    if (vlen == kTombstone) {
      index_.erase(key);
    } else {
      index_[key] = Loc{pos, vlen};  // value starts here
      file_.seekg(vlen, std::ios::cur);
      pos += vlen;
    }
  }
  log_size_ = pos;
  file_.clear();
}

LogKvStore::Loc LogKvStore::append(const std::string& key, codec::ByteView value,
                                   bool tombstone) {
  file_.clear();
  file_.seekp(0, std::ios::end);
  put_u32(file_, static_cast<std::uint32_t>(key.size()));
  file_.write(key.data(), static_cast<std::streamsize>(key.size()));
  Loc loc;
  if (tombstone) {
    put_u32(file_, kTombstone);
    log_size_ += 4 + key.size() + 4;
  } else {
    put_u32(file_, static_cast<std::uint32_t>(value.size()));
    const std::uint64_t value_offset = log_size_ + 4 + key.size() + 4;
    if (!value.empty()) {
      file_.write(reinterpret_cast<const char*>(value.data()),
                  static_cast<std::streamsize>(value.size()));
    }
    loc = Loc{value_offset, static_cast<std::uint32_t>(value.size())};
    log_size_ += 4 + key.size() + 4 + value.size();
  }
  file_.flush();
  return loc;
}

void LogKvStore::put(const std::string& key, codec::ByteView value) {
  index_[key] = append(key, value, /*tombstone=*/false);
}

void LogKvStore::erase(const std::string& key) {
  if (index_.erase(key) == 0) return;  // nothing to delete
  append(key, {}, /*tombstone=*/true);
}

std::optional<Bytes> LogKvStore::get(const std::string& key) const {
  auto it = index_.find(key);
  if (it == index_.end()) return std::nullopt;
  Bytes out(it->second.len);
  file_.clear();
  file_.seekg(static_cast<std::streamoff>(it->second.offset), std::ios::beg);
  if (it->second.len > 0) {
    file_.read(reinterpret_cast<char*>(out.data()), it->second.len);
    if (file_.gcount() != static_cast<std::streamsize>(it->second.len)) {
      throw std::runtime_error("kvstore: short read for key " + key);
    }
  }
  file_.clear();
  return out;
}

std::vector<std::string> LogKvStore::keys() const {
  std::vector<std::string> out;
  out.reserve(index_.size());
  for (const auto& [k, _] : index_) out.push_back(k);
  return out;
}

std::uint64_t LogKvStore::compact() {
  // Read every live value, then rewrite a fresh log with only those records.
  std::map<std::string, Bytes> live;
  for (const auto& [k, _] : index_) live[k] = *get(k);

  const std::uint64_t old_size = log_size_;
  const std::filesystem::path tmp = path_.string() + ".compact";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    for (const auto& [k, v] : live) {
      put_u32(out, static_cast<std::uint32_t>(k.size()));
      out.write(k.data(), static_cast<std::streamsize>(k.size()));
      put_u32(out, static_cast<std::uint32_t>(v.size()));
      if (!v.empty()) out.write(reinterpret_cast<const char*>(v.data()),
                                static_cast<std::streamsize>(v.size()));
    }
  }
  file_.close();
  std::filesystem::rename(tmp, path_);
  file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
  if (!file_) throw std::runtime_error("kvstore: reopen after compact failed");
  replay();  // rebuild the index against the compacted log
  return old_size > log_size_ ? old_size - log_size_ : 0;
}

}  // namespace aviary::storage
