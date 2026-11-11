#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aviary_c.h"
#include "c_abi_support.hpp"

/// The scenario behind vectors/subchain_v2_vectors.txt, driven through the C
/// ABI only so every binding can replay the same calls: a main chain with a
/// dynamic subchain policy, a user chain registered after genesis, anchored,
/// proven, persisted and closed, plus a wider registry for a proof with more
/// than one sibling. Every input is fixed, and every signature scheme used is
/// deterministic, so the outputs are byte for byte reproducible.
namespace subchain_vectors {

using cabi::Bytes;
using cabi::take;
using Vectors = std::vector<std::pair<std::string, std::string>>;

/// The Ed25519 key (seed || public) from the seed 32 x 0xA0: the registrar,
/// and the author of the one record written on the user chain.
inline Bytes registrar_secret() {
  static const char* kHex =
      "a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
      "b533d8ad9fcfbdde0b481c1b334ddc3c53412fd614564e7e5afd020368d382c3";
  Bytes out;
  auto nibble = [](char c) { return static_cast<std::uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
  for (std::size_t i = 0; kHex[i] != 0; i += 2)
    out.push_back(static_cast<std::uint8_t>((nibble(kHex[i]) << 4) | nibble(kHex[i + 1])));
  return out;
}

struct Validator {
  Bytes pub;
  Bytes sec;
};

inline Validator validator(std::uint8_t seed) {
  auto [p, s] = cabi::bls(seed);
  return Validator{p, s};
}

inline Bytes record_genesis(std::uint64_t chain_id, const Validator& v) {
  const std::uint64_t power = 1;
  const Bytes g = take(aviary_genesis_build(chain_id, 1, 1, 2, nullptr, nullptr, 0, v.pub.data(), &power, 1));
  const Bytes bp = take(aviary_blueprint_encode("bft", "record", "", nullptr, 0));
  return take(aviary_genesis_commit_blueprint(g.data(), g.size(), bp.data(), bp.size()));
}

inline AviaryChain assemble(const Bytes& genesis, const Validator& v) {
  AviaryChain c =
      aviary_chain_assemble(genesis.data(), genesis.size(), 0, v.pub.data(), v.pub.size(), v.sec.data(), v.sec.size());
  if (c != nullptr) aviary_chain_archive_enable(c);
  return c;
}

inline Bytes commit(AviaryChain c, const Bytes& block) {
  const Bytes cert = take(aviary_chain_certify(c, block.data(), block.size()));
  aviary_chain_submit(c, block.data(), block.size(), cert.data(), cert.size());
  return block;
}

inline Bytes propose(AviaryChain c, std::uint64_t ts, const std::vector<Bytes>& anchors,
                     const std::vector<Bytes>& ops) {
  const Bytes a = anchors.empty() ? Bytes{} : cabi::list(anchors);
  const Bytes o = ops.empty() ? Bytes{} : cabi::list(ops);
  return take(aviary_chain_propose_subchain(c, ts, a.data(), a.size(), o.data(), o.size()));
}

inline Bytes header_of(const Bytes& block) { return take(aviary_block_header(block.data(), block.size())); }
inline Bytes header_hash(const Bytes& header) {
  return take(aviary_encoded_header_hash(header.data(), header.size()));
}
inline Bytes subchain_root(const Bytes& header) {
  return take(aviary_header_subchain_root(header.data(), header.size()));
}
inline Bytes blake3(const Bytes& b) { return take(aviary_hash(b.data(), b.size())); }

inline Bytes registration_for(std::uint64_t chain_id, const Bytes& genesis, const Validator& v) {
  const std::uint64_t power = 1;
  const Bytes header = take(aviary_genesis_header(genesis.data(), genesis.size()));
  return take(aviary_child_registration_encode(chain_id, header.data(), header.size(), v.pub.data(), &power, 1, 0));
}

inline Bytes signed_op(const Bytes& op, std::uint64_t parent_id, const Bytes& parent_hash, const Bytes& secret) {
  return take(aviary_subchain_op_sign(op.data(), op.size(), parent_id, parent_hash.data(), 0, 1, secret.data(),
                                      secret.size()));
}

/// Runs the scenario and returns every named value, in file order, as hex.
inline Vectors compute() {
  Vectors out;
  auto put = [&](const char* name, const Bytes& value) { out.emplace_back(name, cabi::hex(value)); };

  const Bytes secret = registrar_secret();
  const Bytes registrar_pub(secret.begin() + 32, secret.end());
  const Validator server = validator(0x01), user = validator(0x10);
  put("registrar_secret", secret);

  const Bytes registrar = take(aviary_registrar_encode(1, registrar_pub.data(), registrar_pub.size()));
  const Bytes registrars = cabi::list({registrar});
  const Bytes policy =
      take(aviary_dynamic_subchain_policy_encode(1, 0, 0, 0, 0, 1, registrars.data(), registrars.size(), nullptr, 0));
  put("policy", policy);
  const Bytes main_base = record_genesis(1, server);
  const Bytes main_genesis =
      take(aviary_genesis_commit_subchain_policy(main_base.data(), main_base.size(), policy.data(), policy.size()));
  const Bytes main_hash = take(aviary_genesis_hash(main_genesis.data(), main_genesis.size()));
  put("main_genesis_hash", main_hash);
  put("main_genesis_header", take(aviary_genesis_header(main_genesis.data(), main_genesis.size())));

  const Bytes user_base = record_genesis(1001, user);
  const Bytes user_genesis =
      take(aviary_genesis_commit_parent_binding(user_base.data(), user_base.size(), 1, main_hash.data()));
  const Bytes registration = registration_for(1001, user_genesis, user);
  put("registration", registration);
  const Bytes op_register = take(aviary_subchain_op_register(0, 0, registration.data(), registration.size()));
  put("op_register", op_register);
  put("op_register_digest",
      take(aviary_subchain_op_digest(op_register.data(), op_register.size(), 1, main_hash.data())));
  const Bytes op_register_signed = signed_op(op_register, 1, main_hash, secret);
  put("op_register_signed", op_register_signed);

  AviaryChain main_chain = assemble(main_genesis, server);
  AviaryChain user_chain = assemble(user_genesis, user);
  const Bytes header_1 = header_of(commit(main_chain, propose(main_chain, 10, {}, {op_register_signed})));
  put("header_1", header_1);
  put("header_1_hash", header_hash(header_1));
  put("subchain_root_1", subchain_root(header_1));
  put("child_record_1", take(aviary_chain_child_record(main_chain, 1001)));

  const Bytes key(32, 0x42);
  const std::string value = "door opened";
  const Bytes tx = take(aviary_record_tx_sign(1001, 0, key.data(), 0,
                                              reinterpret_cast<const std::uint8_t*>(value.data()), value.size(),
                                              secret.data(), secret.size()));
  (void)take(aviary_chain_add_tx(user_chain, tx.data(), tx.size()));
  commit(user_chain, take(aviary_chain_propose(user_chain, 20)));
  commit(user_chain, take(aviary_chain_propose(user_chain, 21)));
  const Bytes anchor = take(aviary_chain_pending_anchor(main_chain, user_chain, 0));
  put("anchor", anchor);
  const Bytes header_2 = header_of(commit(main_chain, propose(main_chain, 11, {anchor}, {})));
  put("header_2_hash", header_hash(header_2));
  const Bytes root_2 = subchain_root(header_2);
  put("subchain_root_2", root_2);
  const Bytes record_2 = take(aviary_chain_child_record(main_chain, 1001));
  put("child_record_2", record_2);
  const Bytes proof_2 = take(aviary_chain_child_proof(main_chain, 1001));
  put("child_proof_2", proof_2);
  put("child_proof_2_verified", take(aviary_child_proof_verify(root_2.data(), proof_2.data(), proof_2.size())));
  const Bytes record_proof = take(aviary_chain_prove_record(user_chain, key.data()));
  const Bytes hop = take(aviary_chain_subchain_hop_v2(main_chain, 1001));
  const Bytes hops = cabi::list({hop});
  put("path_proof_2", take(aviary_subchain_proof_v2_encode(hops.data(), hops.size(), record_proof.data(),
                                                           record_proof.size())));
  put("main_snapshot_hash_2", blake3(take(aviary_chain_snapshot(main_chain))));
  put("user_snapshot_hash_2", blake3(take(aviary_chain_snapshot(user_chain))));

  const Bytes tip_hash(record_2.begin() + 70, record_2.begin() + 102);
  const Bytes op_close = take(aviary_subchain_op_close(0, 0, 1001, cabi::be64(record_2, 62), tip_hash.data()));
  put("op_close", op_close);
  put("op_close_digest", take(aviary_subchain_op_digest(op_close.data(), op_close.size(), 1, main_hash.data())));
  const Bytes header_3 = header_of(commit(main_chain, propose(main_chain, 12, {}, {signed_op(op_close, 1, main_hash, secret)})));
  put("header_3_hash", header_hash(header_3));
  put("subchain_root_3", subchain_root(header_3));
  put("child_record_3", take(aviary_chain_child_record(main_chain, 1001)));
  put("main_snapshot_hash_3", blake3(take(aviary_chain_snapshot(main_chain))));
  aviary_chain_free(user_chain);
  aviary_chain_free(main_chain);

  const Bytes wide_base = record_genesis(2, server);
  const Bytes wide_genesis =
      take(aviary_genesis_commit_subchain_policy(wide_base.data(), wide_base.size(), policy.data(), policy.size()));
  const Bytes wide_hash = take(aviary_genesis_hash(wide_genesis.data(), wide_genesis.size()));
  AviaryChain wide = assemble(wide_genesis, server);
  std::vector<Bytes> ops;
  for (std::uint64_t id = 2000; id < 2011; ++id) {
    const Bytes g = record_genesis(id, user);
    const Bytes reg = registration_for(id, g, user);
    ops.push_back(signed_op(take(aviary_subchain_op_register(0, 0, reg.data(), reg.size())), 2, wide_hash, secret));
  }
  const Bytes wide_header = header_of(commit(wide, propose(wide, 10, {}, ops)));
  const Bytes wide_root = subchain_root(wide_header);
  put("wide_root", wide_root);
  const Bytes wide_proof = take(aviary_chain_child_proof(wide, 2007));
  put("wide_proof", wide_proof);
  put("wide_proof_verified", take(aviary_child_proof_verify(wide_root.data(), wide_proof.data(), wide_proof.size())));
  aviary_chain_free(wide);
  return out;
}

}  // namespace subchain_vectors
