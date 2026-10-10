// libFuzzer harness for the C ABI entry points that parse bytes coming from
// outside the process: snapshots, encoded transactions, transaction lists,
// gossip payloads, secure session handshakes and frames, key store blobs,
// chain tree specs and portable envelopes. Every call must return an error or
// NULL on bad input; a crash or a sanitizer report is a bug.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "aviary_c.h"

namespace {
struct Fixture {
  AviaryNode node = nullptr;
  AviaryNetNode net = nullptr;
  AviarySecureSession open_session = nullptr;
  std::vector<std::uint8_t> kem_pub, kem_sec, sig_pub, sig_sec;

  static std::vector<std::uint8_t> take(AviaryBufferResult r) {
    std::vector<std::uint8_t> out;
    if (r.buf.data) {
      out.assign(r.buf.data, r.buf.data + r.buf.len);
      aviary_buffer_free(&r.buf);
    }
    if (r.error) aviary_str_free(r.error);
    return out;
  }
  static std::vector<std::vector<std::uint8_t>> split(const std::vector<std::uint8_t>& b) {
    std::vector<std::vector<std::uint8_t>> out;
    if (b.size() < 4) return out;
    auto u32 = [&](std::size_t o) { return (std::size_t(b[o]) << 24) | (std::size_t(b[o + 1]) << 16) | (std::size_t(b[o + 2]) << 8) | b[o + 3]; };
    std::size_t n = u32(0), off = 4;
    for (std::size_t i = 0; i < n && off + 4 <= b.size(); ++i) {
      std::size_t l = u32(off); off += 4;
      if (off + l > b.size()) break;
      out.emplace_back(b.begin() + off, b.begin() + off + l); off += l;
    }
    return out;
  }
  Fixture() {
    aviary_init();
    node = aviary_node_new(77);
    std::vector<std::uint8_t> ikm(32, 0x61);
    auto key = split(take(aviary_bls_keygen(ikm.data(), ikm.size())));
    std::vector<std::uint8_t> pk = take(aviary_bls_keygen(ikm.data(), ikm.size()));
    // bls_keygen returns [pub_len u32][pub][sec]
    std::size_t pl = (std::size_t(pk[0]) << 24) | (std::size_t(pk[1]) << 16) | (std::size_t(pk[2]) << 8) | pk[3];
    std::vector<std::uint8_t> pub(pk.begin() + 4, pk.begin() + 4 + pl), sec(pk.begin() + 4 + pl, pk.end());
    std::uint64_t power = 1;
    std::vector<std::uint8_t> genesis = take(aviary_genesis_build(42, 1, 1, 0, nullptr, nullptr, 0, pub.data(), &power, 1));
    net = aviary_net_node_new(genesis.data(), genesis.size(), 0, pub.data(), pub.size(), sec.data(), sec.size(), 1);
    if (net) {
      aviary_net_node_add_peer(net, 1);
      take(aviary_net_node_start(net));
    }
    auto id = split(take(aviary_secure_identity_generate()));
    if (id.size() == 4) { kem_pub = id[0]; kem_sec = id[1]; sig_pub = id[2]; sig_sec = id[3]; }
    AviarySecureSession init = aviary_secure_initiate(kem_pub.data(), kem_pub.size(), sig_pub.data(), sig_pub.size(), sig_sec.data(), sig_sec.size());
    if (init) {
      auto msg1 = take(aviary_secure_handshake_message(init));
      open_session = aviary_secure_accept(kem_pub.data(), kem_pub.size(), kem_sec.data(), kem_sec.size(), sig_pub.data(), sig_pub.size(), sig_sec.data(), sig_sec.size(), msg1.data(), msg1.size());
      aviary_secure_free(init);
    }
  }
};
Fixture& fixture() {
  static Fixture f;
  return f;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  Fixture& f = fixture();
  AviaryNode restored = aviary_node_restore(data, size);
  if (restored) aviary_node_free(restored);
  Fixture::take(aviary_node_submit_tx(f.node, data, size));
  if (f.net) {
    aviary_net_node_set_transactions(f.net, data, size);
    Fixture::take(aviary_net_node_on_gossip(f.net, 1, data, size));
  }
  AviarySecureSession accepted = aviary_secure_accept(f.kem_pub.data(), f.kem_pub.size(), f.kem_sec.data(), f.kem_sec.size(),
                                                      f.sig_pub.data(), f.sig_pub.size(), f.sig_sec.data(), f.sig_sec.size(), data, size);
  if (accepted) aviary_secure_free(accepted);
  AviarySecureSession init = aviary_secure_initiate(f.kem_pub.data(), f.kem_pub.size(), f.sig_pub.data(), f.sig_pub.size(), f.sig_sec.data(), f.sig_sec.size());
  if (init) { aviary_secure_complete(init, data, size); aviary_secure_free(init); }
  if (f.open_session) Fixture::take(aviary_secure_decrypt(f.open_session, data, size));
  AviaryKeyStore ks = aviary_keystore_load(data, size);
  if (ks) aviary_keystore_free(ks);
  AviaryChainTree tree = aviary_chain_tree_build(data, size);
  if (tree) aviary_chain_tree_free(tree);
  Fixture::take(aviary_chain_tree_spec_error(data, size));
  Fixture::take(aviary_portable_open_passphrase(data, size, "pw"));
  Fixture::take(aviary_threshold_share_unseal(data, size, "pw"));
  Fixture::take(aviary_subchain_policy_encode(4, 1, 0, 0, data, size));
  AviaryLightClient lc = aviary_light_client_new(42, data, size, data, size);
  if (lc) aviary_light_client_free(lc);
  return 0;
}
