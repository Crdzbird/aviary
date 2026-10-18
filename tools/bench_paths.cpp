#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "aviary_c.h"

namespace {
using Bytes = std::vector<std::uint8_t>;
using clk = std::chrono::steady_clock;

Bytes take(AviaryBufferResult r) {
  if (r.error != nullptr) {
    std::string e = r.error;
    aviary_str_free(r.error);
    throw std::runtime_error(e);
  }
  Bytes out(r.buf.data, r.buf.data + r.buf.len);
  aviary_buffer_free(&r.buf);
  return out;
}

std::uint32_t be32(const std::uint8_t* p) {
  return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3];
}

void put32(Bytes& b, std::uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) b.push_back(std::uint8_t(v >> s));
}

std::vector<Bytes> blobs(const Bytes& b) {
  std::vector<Bytes> out;
  std::size_t at = 4;
  for (std::uint32_t i = 0, n = be32(b.data()); i < n; ++i) {
    const std::uint32_t len = be32(b.data() + at);
    out.emplace_back(b.begin() + at + 4, b.begin() + at + 4 + len);
    at += 4 + len;
  }
  return out;
}

Bytes blob_list(const std::vector<Bytes>& items) {
  Bytes b;
  put32(b, std::uint32_t(items.size()));
  for (const auto& i : items) {
    put32(b, std::uint32_t(i.size()));
    b.insert(b.end(), i.begin(), i.end());
  }
  return b;
}

double ms(const std::function<void()>& f) {
  const auto t = clk::now();
  f();
  return std::chrono::duration<double, std::milli>(clk::now() - t).count();
}

void row(const char* path, const char* work, std::size_t n, double total_ms) {
  const double per_us = total_ms * 1000.0 / double(n);
  std::printf("| %s | %s | %zu | %.1f ms | %.1f us | %.0f /s |\n", path, work, n, total_ms, per_us,
              1e6 / per_us);
}

struct Account {
  Bytes secret, pub, address;
};

Account ed_account(AviaryNode node) {
  Account a;
  a.secret = take(aviary_node_new_account(node));
  a.pub.assign(a.secret.begin() + 32, a.secret.end());
  a.address = take(aviary_account_address(a.pub.data(), a.pub.size()));
  return a;
}

std::vector<Account> funded(AviaryNode node, std::size_t n) {
  std::vector<Account> out;
  for (std::size_t i = 0; i < n; ++i) {
    out.push_back(ed_account(node));
    aviary_node_credit(node, out.back().address.data(), 1000000);
  }
  return out;
}

void produce(AviaryNode node, std::uint64_t ts) {
  std::uint64_t h = 0;
  if (aviary_node_produce_block(node, ts, &h) != 0) throw std::runtime_error("produce_block");
}

constexpr std::size_t kTx = 1000;

void ledger() {
  AviaryNode node = aviary_node_new(1);
  auto accounts = funded(node, kTx);
  const Bytes& to = accounts[0].address;
  double admit = 0, block = 0;
  for (std::uint64_t r = 0; r < 3; ++r) {
    admit += ms([&] {
      for (const auto& a : accounts)
        take(aviary_node_submit_transfer(node, a.secret.data(), a.secret.size(), to.data(), 1, r));
    });
    block += ms([&] { produce(node, r + 2); });
  }
  row("plain", "Ed25519 transfer, sign and admit", 3 * kTx, admit);
  row("plain", "BFT block of 1,000, validate, apply, certify", 3 * kTx, block);
  const Bytes snap = take(aviary_node_snapshot(node));
  row("plain", "Follower replay, all signatures and certificates", 3 * kTx,
      ms([&] { aviary_node_free(aviary_node_restore_replay(snap.data(), snap.size())); }));
  aviary_node_free(node);

  AviaryNode pq = aviary_node_new(2);
  constexpr std::size_t kHybrid = 100;
  std::vector<std::pair<Bytes, Bytes>> keys;
  for (std::size_t i = 0; i < kHybrid; ++i) {
    const Bytes blob = take(aviary_node_new_account_hybrid(pq));
    const std::uint32_t pl = be32(blob.data());
    Bytes pub(blob.begin() + 4, blob.begin() + 4 + pl), sec(blob.begin() + 4 + pl, blob.end());
    const Bytes addr = take(aviary_account_address(pub.data(), pub.size()));
    aviary_node_credit(pq, addr.data(), 1000);
    keys.emplace_back(std::move(pub), std::move(sec));
  }
  row("plain", "Hybrid Ed25519 + ML-DSA-65 transfer, sign and admit", kHybrid, ms([&] {
        for (const auto& [pub, sec] : keys)
          take(aviary_node_submit_transfer_hybrid(pq, sec.data(), sec.size(), pub.data(), pub.size(),
                                                  to.data(), 1, 0));
      }));
  aviary_node_free(pq);
}

void keystore() {
  AviaryNode node = aviary_node_new(3);
  auto accounts = funded(node, kTx);
  const Bytes& to = accounts[0].address;
  AviaryKeyStore ks = nullptr;
  row("key store", "Create the store (Argon2id key derivation)", 1,
      ms([&] { ks = aviary_keystore_new("bench passphrase"); }));
  std::vector<Bytes> addrs;
  row("key store", "Seal an account into the store", kTx, ms([&] {
        for (const auto& a : accounts) {
          Bytes addr(32);
          aviary_keystore_add_account(ks, a.pub.data(), a.pub.size(), a.secret.data(), a.secret.size(),
                                      addr.data());
          addrs.push_back(std::move(addr));
        }
      }));
  row("raw secret", "Ed25519 transfer, sign and admit", kTx, ms([&] {
        for (const auto& a : accounts)
          take(aviary_node_submit_transfer(node, a.secret.data(), a.secret.size(), to.data(), 1, 0));
      }));
  produce(node, 2);
  row("key store", "Ed25519 transfer, unseal, sign and admit", kTx, ms([&] {
        for (const auto& a : addrs)
          take(aviary_node_submit_transfer_keystore(node, ks, a.data(), to.data(), 1, 1, 0));
      }));
  produce(node, 3);
  const Bytes blob = take(aviary_keystore_serialise(ks));
  AviaryKeyStore back = aviary_keystore_load(blob.data(), blob.size());
  row("key store", "Unlock a saved store (Argon2id)", 1,
      ms([&] { aviary_keystore_unlock(back, "bench passphrase"); }));
  aviary_keystore_free(back);
  aviary_keystore_free(ks);
  aviary_node_free(node);
}

void sealed_mempool() {
  AviaryNode node = aviary_node_new(4);
  auto accounts = funded(node, kTx);
  AviaryKeyStore ks = aviary_keystore_new("bench passphrase");
  std::vector<Bytes> addrs;
  for (const auto& a : accounts) {
    Bytes addr(32);
    aviary_keystore_add_account(ks, a.pub.data(), a.pub.size(), a.secret.data(), a.secret.size(), addr.data());
    addrs.push_back(std::move(addr));
  }
  auto signed_at = [&](std::uint64_t nonce) {
    std::vector<Bytes> txs;
    for (const auto& a : addrs)
      txs.push_back(take(aviary_keystore_sign_transfer(ks, 4, a.data(), addrs[0].data(), 1, nonce, 0)));
    return txs;
  };
  const auto plain = signed_at(0), hidden = signed_at(1);
  const Bytes kem = take(aviary_hybrid_kem_keygen());
  const std::uint32_t pl = be32(kem.data());
  const Bytes kpub(kem.begin() + 4, kem.begin() + 4 + pl), ksec(kem.begin() + 4 + pl, kem.end());

  const double p_admit = ms([&] {
    for (const auto& t : plain) take(aviary_node_submit_tx(node, t.data(), t.size()));
  });
  const double p_block = ms([&] { produce(node, 2); });
  row("plain mempool", "Admit a signed transaction", kTx, p_admit);
  row("plain mempool", "Admit and finalize", kTx, p_admit + p_block);

  std::vector<Bytes> sealed;
  const double seal = ms([&] {
    for (const auto& t : hidden) sealed.push_back(take(aviary_sealed_tx_seal(t.data(), t.size(), kpub.data(), kpub.size())));
  });
  AviarySealedMempool pool = aviary_sealed_mempool_new(kTx * 2);
  const double add = ms([&] {
    for (const auto& s : sealed)
      if (aviary_sealed_mempool_add(pool, s.data(), s.size()) != 1) throw std::runtime_error("sealed add");
  });
  std::vector<Bytes> opened;
  const double reveal = ms([&] { opened = blobs(take(aviary_sealed_mempool_reveal(pool, ksec.data(), ksec.size(), kTx))); });
  if (opened.size() != kTx) throw std::runtime_error("sealed reveal count");
  const double s_admit = ms([&] {
    for (const auto& t : opened) take(aviary_node_submit_tx(node, t.data(), t.size()));
  });
  const double s_block = ms([&] { produce(node, 3); });
  row("sealed mempool", "Seal to the proposer (client side)", kTx, seal);
  row("sealed mempool", "Admit sealed, no decryption", kTx, add);
  row("sealed mempool", "Proposer opens the batch", kTx, reveal);
  row("sealed mempool", "Seal, admit, open, admit and finalize", kTx, seal + add + reveal + s_admit + s_block);
  std::printf("| sealed mempool | Size of a sealed transfer | 1 | %zu bytes against %zu plain | | |\n",
              sealed[0].size(), hidden[0].size());
  aviary_sealed_mempool_free(pool);
  aviary_keystore_free(ks);
  aviary_node_free(node);
}

enum class Link { kPlain, kNoise, kSecure };

struct Channel {
  std::function<Bytes(const Bytes&)> seal, open;
};

struct Mesh {
  static constexpr std::uint32_t kN = 4;
  std::vector<AviaryNetNode> nodes;
  std::map<std::pair<std::uint32_t, std::uint32_t>, Channel> links;
  std::vector<std::function<void()>> cleanup;
  std::deque<std::tuple<std::uint32_t, std::uint32_t, Bytes>> queue;
  std::size_t frames = 0, wire_bytes = 0;

  void apply(std::uint32_t from, const Bytes& out) {
    std::size_t at = 4;
    for (std::uint32_t i = 0, n = be32(out.data()); i < n; ++i) {
      const std::uint32_t to = be32(out.data() + at), len = be32(out.data() + at + 4);
      queue.emplace_back(from, to, Bytes(out.begin() + at + 8, out.begin() + at + 8 + len));
      at += 8 + len;
    }
  }

  void settle() {
    while (!queue.empty()) {
      auto [from, to, payload] = std::move(queue.front());
      queue.pop_front();
      const Channel& c = links.at({from, to});
      const Bytes wire = c.seal(payload);
      ++frames;
      wire_bytes += wire.size();
      const Bytes got = c.open(wire);
      apply(to, take(aviary_net_node_on_gossip(nodes[to], from, got.data(), got.size())));
    }
  }

  void run_height(bool first) {
    for (std::uint32_t i = 0; i < kN; ++i)
      apply(i, take(first ? aviary_net_node_start(nodes[i]) : aviary_net_node_advance(nodes[i])));
    settle();
    for (auto* n : nodes)
      if (aviary_net_node_committed(n) != 1) throw std::runtime_error("mesh did not commit");
  }

  ~Mesh() {
    for (auto& f : cleanup) f();
    for (auto* n : nodes) aviary_net_node_free(n);
  }
};

void wire(Mesh& m, Link link, std::uint32_t a, std::uint32_t b, const std::vector<Bytes>& ids) {
  if (link == Link::kPlain) {
    const Channel c{[](const Bytes& p) { return p; }, [](const Bytes& p) { return p; }};
    m.links[{a, b}] = c;
    m.links[{b, a}] = c;
    return;
  }
  if (link == Link::kNoise) {
    const Bytes pro{'a', 'v', 'i'};
    AviaryNoiseSession i = aviary_noise_initiate(ids[a].data(), ids[a].size(), pro.data(), pro.size());
    const Bytes m0 = take(aviary_noise_handshake_message(i));
    AviaryNoiseSession r = aviary_noise_accept(ids[b].data(), ids[b].size(), m0.data(), m0.size(), pro.data(), pro.size());
    const Bytes m1 = take(aviary_noise_handshake_message(r));
    const Bytes m2 = take(aviary_noise_complete_initiator(i, m1.data(), m1.size()));
    aviary_noise_complete_responder(r, m2.data(), m2.size());
    const Bytes pi = take(aviary_noise_identity_proof(i)), pr = take(aviary_noise_identity_proof(r));
    aviary_noise_verify_identity_proof(r, pi.data(), pi.size());
    aviary_noise_verify_identity_proof(i, pr.data(), pr.size());
    if (aviary_noise_open(i) != 1 || aviary_noise_open(r) != 1) throw std::runtime_error("noise not open");
    auto enc = [](AviaryNoiseSession s) { return [s](const Bytes& p) { return take(aviary_noise_encrypt(s, p.data(), p.size())); }; };
    auto dec = [](AviaryNoiseSession s) { return [s](const Bytes& p) { return take(aviary_noise_decrypt(s, p.data(), p.size())); }; };
    m.links[{a, b}] = Channel{enc(i), dec(r)};
    m.links[{b, a}] = Channel{enc(r), dec(i)};
    m.cleanup.push_back([i, r] { aviary_noise_free(i); aviary_noise_free(r); });
    return;
  }
  const auto ka = blobs(ids[a]), kb = blobs(ids[b]);
  AviarySecureSession i = aviary_secure_initiate(kb[0].data(), kb[0].size(), ka[2].data(), ka[2].size(), ka[3].data(), ka[3].size());
  const Bytes m1 = take(aviary_secure_handshake_message(i));
  AviarySecureSession r = aviary_secure_accept(kb[0].data(), kb[0].size(), kb[1].data(), kb[1].size(), kb[2].data(),
                                               kb[2].size(), kb[3].data(), kb[3].size(), m1.data(), m1.size());
  const Bytes m2 = take(aviary_secure_handshake_message(r));
  if (aviary_secure_complete(i, m2.data(), m2.size()) != 1) throw std::runtime_error("secure not open");
  auto enc = [](AviarySecureSession s) { return [s](const Bytes& p) { return take(aviary_secure_encrypt(s, p.data(), p.size())); }; };
  auto dec = [](AviarySecureSession s) { return [s](const Bytes& p) { return take(aviary_secure_decrypt(s, p.data(), p.size())); }; };
  m.links[{a, b}] = Channel{enc(i), dec(r)};
  m.links[{b, a}] = Channel{enc(r), dec(i)};
  m.cleanup.push_back([i, r] { aviary_secure_free(i); aviary_secure_free(r); });
}

void mesh(Link link, const char* name) {
  constexpr int kHeights = 20;
  Mesh m;
  Bytes pks;
  std::vector<std::uint64_t> powers(Mesh::kN, 1);
  std::vector<Bytes> bls;
  for (std::uint32_t i = 0; i < Mesh::kN; ++i) {
    const Bytes ikm(32, std::uint8_t(0x70 + i));
    bls.push_back(take(aviary_bls_keygen(ikm.data(), ikm.size())));
    pks.insert(pks.end(), bls.back().begin() + 4, bls.back().begin() + 52);
  }
  const Bytes genesis = take(aviary_genesis_build(5, 1, 1, 0, nullptr, nullptr, 0, pks.data(), powers.data(), Mesh::kN));
  for (std::uint32_t i = 0; i < Mesh::kN; ++i)
    m.nodes.push_back(aviary_net_node_new(genesis.data(), genesis.size(), i, bls[i].data() + 4, 48, bls[i].data() + 52, 32, 1));
  std::vector<Bytes> ids;
  for (std::uint32_t i = 0; i < Mesh::kN; ++i)
    ids.push_back(link == Link::kSecure ? take(aviary_secure_identity_generate())
                  : link == Link::kNoise ? take(aviary_noise_identity_generate())
                                         : Bytes{});
  const double hs = ms([&] {
    for (std::uint32_t a = 0; a < Mesh::kN; ++a)
      for (std::uint32_t b = a + 1; b < Mesh::kN; ++b) {
        aviary_net_node_add_peer(m.nodes[a], b);
        aviary_net_node_add_peer(m.nodes[b], a);
        wire(m, link, a, b, ids);
      }
  });
  if (link != Link::kPlain) row(name, "Handshake per link, both sides", 6, hs);
  const double t = ms([&] {
    for (int h = 0; h < kHeights; ++h) m.run_height(h == 0);
  });
  row(name, "4 validator BFT height, full mesh", kHeights, t);
  std::printf("| %s | Frames and wire bytes per height | %zu | %zu bytes | | |\n", name, m.frames / kHeights,
              m.wire_bytes / kHeights);
}

void frames() {
  const Bytes payload(1024, 0x5A);
  for (Link link : {Link::kNoise, Link::kSecure}) {
    Mesh m;
    std::vector<Bytes> ids = {link == Link::kSecure ? take(aviary_secure_identity_generate()) : take(aviary_noise_identity_generate()),
                              link == Link::kSecure ? take(aviary_secure_identity_generate()) : take(aviary_noise_identity_generate())};
    wire(m, link, 0, 1, ids);
    const Channel& c = m.links.at({0, 1});
    constexpr std::size_t kFrames = 20000;
    row(link == Link::kSecure ? "SecureSession" : "Noise", "Encrypt and decrypt a 1 KiB frame", kFrames, ms([&] {
          for (std::size_t i = 0; i < kFrames; ++i) c.open(c.seal(payload));
        }));
  }
}
}

int main() {
  if (aviary_init() != 0) return 1;
  std::printf("| Path | Work | Items | Time | Per item | Rate |\n|---|---|---|---|---|---|\n");
  ledger();
  keystore();
  sealed_mempool();
  mesh(Link::kPlain, "plaintext links");
  mesh(Link::kNoise, "Noise links");
  mesh(Link::kSecure, "SecureSession links");
  frames();
  return 0;
}
