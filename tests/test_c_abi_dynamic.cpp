#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "aviary_c.h"
#include "c_abi_support.hpp"
#include "check.hpp"

using cabi::Bytes;
using cabi::take;
using cabi::take_error;

namespace {
struct Keys {
  Bytes pub;
  Bytes sec;
};

Keys keys(std::uint8_t seed) {
  auto [p, s] = cabi::bls(seed);
  return Keys{p, s};
}

Bytes ed_secret(std::uint8_t tag) {
  AviaryNode n = aviary_node_new(700 + tag);
  Bytes s = take(aviary_node_new_account(n));
  aviary_node_free(n);
  return s;
}

Bytes record_genesis(std::uint64_t chain_id, const Keys& k) {
  const std::uint64_t power = 1;
  const Bytes g = take(aviary_genesis_build(chain_id, 1, 1, 2, nullptr, nullptr, 0, k.pub.data(), &power, 1));
  const Bytes bp = take(aviary_blueprint_encode("bft", "record", "", nullptr, 0));
  return take(aviary_genesis_commit_blueprint(g.data(), g.size(), bp.data(), bp.size()));
}

AviaryChain assemble(const Bytes& genesis, const Keys& k) {
  return aviary_chain_assemble(genesis.data(), genesis.size(), 0, k.pub.data(), k.pub.size(), k.sec.data(),
                               k.sec.size());
}

Bytes commit(AviaryChain c, const Bytes& block) {
  const Bytes cert = take(aviary_chain_certify(c, block.data(), block.size()));
  CHECK(aviary_chain_submit(c, block.data(), block.size(), cert.data(), cert.size()) == 1);
  return block;
}

Bytes propose(AviaryChain c, std::uint64_t ts, const std::vector<Bytes>& anchors, const std::vector<Bytes>& ops) {
  const Bytes a = anchors.empty() ? Bytes{} : cabi::list(anchors);
  const Bytes o = ops.empty() ? Bytes{} : cabi::list(ops);
  return take(aviary_chain_propose_subchain(c, ts, a.data(), a.size(), o.data(), o.size()));
}

std::string propose_error(AviaryChain c, std::uint64_t ts, const std::vector<Bytes>& anchors,
                          const std::vector<Bytes>& ops) {
  const Bytes a = anchors.empty() ? Bytes{} : cabi::list(anchors);
  const Bytes o = ops.empty() ? Bytes{} : cabi::list(ops);
  return take_error(aviary_chain_propose_subchain(c, ts, a.data(), a.size(), o.data(), o.size()));
}

struct Record {
  std::uint32_t index;
  std::uint64_t chain_id;
  std::uint8_t status;
  std::uint64_t registered_height, closed_height, tip_height;
  Bytes genesis_header_hash, tip_hash, tip_state_root;
};

Record parse(const Bytes& b) {
  Record r{};
  if (b.size() != 230) return r;
  r.index = cabi::be32(b, 0);
  r.chain_id = cabi::be64(b, 4);
  r.status = b[12];
  r.genesis_header_hash = Bytes(b.begin() + 14, b.begin() + 46);
  r.registered_height = cabi::be64(b, 46);
  r.closed_height = cabi::be64(b, 54);
  r.tip_height = cabi::be64(b, 62);
  r.tip_hash = Bytes(b.begin() + 70, b.begin() + 102);
  r.tip_state_root = Bytes(b.begin() + 102, b.begin() + 134);
  return r;
}

Bytes header_hash(const Bytes& header) { return take(aviary_encoded_header_hash(header.data(), header.size())); }
Bytes root_of(AviaryChain c) {
  const Bytes tip = take(aviary_chain_tip(c));
  return take(aviary_header_subchain_root(tip.data(), tip.size()));
}
}  // namespace

int main() {
  CHECK(aviary_init() == 0);
  const Keys server = keys(0x01), user_key = keys(0x10), other_key = keys(0x11);
  const Bytes registrar = ed_secret(1), second = ed_secret(2), stranger = ed_secret(3);
  const Bytes registrar_pub(registrar.begin() + 32, registrar.end());
  const Bytes second_pub(second.begin() + 32, second.end());

  std::printf("== a main chain with a dynamic policy, built through the C ABI ==\n");
  const Bytes reg0 = take(aviary_registrar_encode(1, registrar_pub.data(), registrar_pub.size()));
  const Bytes reg1 = take(aviary_registrar_encode(1, second_pub.data(), second_pub.size()));
  const Bytes registrars = cabi::list({reg0});
  const Bytes policy = take(aviary_dynamic_subchain_policy_encode(1, 0, 0, 0, 0, 1, registrars.data(),
                                                                 registrars.size(), nullptr, 0));
  CHECK(policy.size() > 4 && policy[0] == 0 && policy[1] == 0 && policy[2] == 0 && policy[3] == 2);
  const Bytes main_genesis = [&] {
    const Bytes g = record_genesis(1, server);
    return take(aviary_genesis_commit_subchain_policy(g.data(), g.size(), policy.data(), policy.size()));
  }();
  const Bytes main_hash = take(aviary_genesis_hash(main_genesis.data(), main_genesis.size()));
  CHECK(!main_genesis.empty() && main_hash.size() == 32);
  {
    const Bytes none = cabi::list({});
    const Bytes bad = take(aviary_dynamic_subchain_policy_encode(0, 0, 0, 0, 0, 1, registrars.data(),
                                                                 registrars.size(), none.data(), none.size()));
    const Bytes g = record_genesis(1, server);
    CHECK(!take_error(aviary_genesis_commit_subchain_policy(g.data(), g.size(), bad.data(), bad.size())).empty());
    CHECK(!take_error(aviary_dynamic_subchain_policy_encode(1, 0, 0, 0, 0, 1, policy.data(), policy.size(), nullptr, 0))
               .empty());
    CHECK(!take_error(aviary_registrar_encode(1, nullptr, 0)).empty());
  }
  AviaryChain main_chain = assemble(main_genesis, server);
  CHECK(main_chain != nullptr && aviary_chain_archive_enable(main_chain) == 1);
  std::uint32_t records = 9, open = 9, epoch = 9;
  CHECK(aviary_chain_registry_counts(main_chain, &records, &open, &epoch) == 1 && records == 0 && open == 0 && epoch == 0);
  {
    const Bytes tip = take(aviary_chain_tip(main_chain));
    CHECK(!tip.empty() && tip[0] == 4);
  }

  std::printf("== a user chain is created, bound to the main chain and registered after genesis ==\n");
  const Bytes user_genesis = [&] {
    const Bytes g = record_genesis(1001, user_key);
    return take(aviary_genesis_commit_parent_binding(g.data(), g.size(), 1, main_hash.data()));
  }();
  AviaryChain user = assemble(user_genesis, user_key);
  CHECK(user != nullptr && aviary_chain_archive_enable(user) == 1);
  const Bytes user_header = take(aviary_genesis_header(user_genesis.data(), user_genesis.size()));
  const std::uint64_t power = 1;
  const Bytes registration = take(aviary_child_registration_encode(1001, user_header.data(), user_header.size(),
                                                                   user_key.pub.data(), &power, 1, 0));
  const Bytes unsigned_op = take(aviary_subchain_op_register(0, 0, registration.data(), registration.size()));
  const Bytes digest = take(aviary_subchain_op_digest(unsigned_op.data(), unsigned_op.size(), 1, main_hash.data()));
  CHECK(digest.size() == 32);
  const Bytes signed_op = take(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 1, main_hash.data(), 0,
                                                       1, registrar.data(), registrar.size()));
  CHECK(signed_op.size() > unsigned_op.size());
  CHECK(take(aviary_subchain_op_digest(signed_op.data(), signed_op.size(), 1, main_hash.data())) == digest);
  CHECK(!take_error(aviary_subchain_op_sign(signed_op.data(), signed_op.size(), 1, main_hash.data(), 0, 1,
                                            registrar.data(), registrar.size()))
             .empty());
  CHECK(!take_error(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 1, main_hash.data(), 0, 1,
                                            registrar.data(), 32))
             .empty());
  CHECK(!take_error(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 1, main_hash.data(), 0, 3,
                                            registrar.data(), registrar.size()))
             .empty());

  CHECK(propose_error(main_chain, 10, {}, {unsigned_op}).find("not enough approvals") != std::string::npos);
  {
    const Bytes forged = take(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 1, main_hash.data(), 0, 1,
                                                      stranger.data(), stranger.size()));
    CHECK(propose_error(main_chain, 10, {}, {forged}).find("bad registrar signature") != std::string::npos);
    Bytes other_parent = main_hash;
    other_parent[0] ^= 1;
    const Bytes replayed = take(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 1,
                                                        other_parent.data(), 0, 1, registrar.data(), registrar.size()));
    CHECK(propose_error(main_chain, 10, {}, {replayed}).find("bad registrar signature") != std::string::npos);
    const Bytes other_id = take(aviary_subchain_op_sign(unsigned_op.data(), unsigned_op.size(), 2, main_hash.data(), 0,
                                                        1, registrar.data(), registrar.size()));
    CHECK(propose_error(main_chain, 10, {}, {other_id}).find("bad registrar signature") != std::string::npos);
    CHECK(!propose_error(main_chain, 10, {}, {Bytes{1, 2, 3}}).empty());
  }
  CHECK(aviary_chain_height(main_chain) == 0);
  const Bytes block1 = commit(main_chain, propose(main_chain, 10, {}, {signed_op}));
  CHECK(aviary_chain_height(main_chain) == 1);
  CHECK(aviary_chain_registry_counts(main_chain, &records, &open, &epoch) == 1 && records == 1 && open == 1);
  Record rec = parse(take(aviary_chain_child_record(main_chain, 1001)));
  CHECK(rec.index == 0 && rec.chain_id == 1001 && rec.status == 1 && rec.registered_height == 1 && rec.tip_height == 0);
  CHECK(rec.genesis_header_hash == header_hash(user_header) && rec.tip_hash == rec.genesis_header_hash);
  CHECK(take(aviary_chain_child_tip(main_chain, 1001)) == user_header);
  CHECK(!take_error(aviary_chain_child_record(main_chain, 7)).empty());
  CHECK(!take_error(aviary_chain_child_record(user, 7)).empty());
  CHECK(aviary_chain_registry_counts(user, &records, &open, &epoch) == 0);
  CHECK(propose_error(main_chain, 11, {}, {signed_op}).find("already registered") != std::string::npos);
  CHECK(!take_error(aviary_chain_subchain_hop(main_chain, 1001)).empty());

  std::printf("== user activity is anchored into the main chain ==\n");
  const Bytes author = ed_secret(9);
  Bytes key(32, 0x42);
  const std::string value = "{\"event\":\"door opened\"}";
  const Bytes tx = take(aviary_record_tx_sign(1001, 0, key.data(), 0,
                                              reinterpret_cast<const std::uint8_t*>(value.data()), value.size(),
                                              author.data(), author.size()));
  CHECK(take(aviary_chain_add_tx(user, tx.data(), tx.size())).size() == 32);
  commit(user, take(aviary_chain_propose(user, 20)));
  commit(user, take(aviary_chain_propose(user, 21)));
  CHECK(take(aviary_chain_pending_anchor(main_chain, user, 1)).size() <
        take(aviary_chain_pending_anchor(main_chain, user, 0)).size());
  const Bytes anchor = take(aviary_chain_pending_anchor(main_chain, user, 0));
  CHECK(!anchor.empty());
  const Bytes block2 = commit(main_chain, propose(main_chain, 11, {anchor}, {}));
  CHECK(take(aviary_chain_pending_anchor(main_chain, user, 0)).empty());
  rec = parse(take(aviary_chain_child_record(main_chain, 1001)));
  const Bytes user_tip = take(aviary_chain_tip(user));
  CHECK(rec.tip_height == 2 && rec.tip_hash == header_hash(user_tip));
  CHECK(rec.tip_state_root == take(aviary_chain_state_root(user)));
  CHECK(take(aviary_chain_child_tip(main_chain, 1001)) == user_tip);
  CHECK(!take_error(aviary_chain_pending_anchor(main_chain, main_chain, 0)).empty() ||
        take(aviary_chain_pending_anchor(main_chain, main_chain, 0)).empty());
  CHECK(!take_error(aviary_chain_pending_anchor(user, main_chain, 0)).empty());

  std::printf("== one user chain is proven against the main chain header ==\n");
  const Bytes proof = take(aviary_chain_child_proof(main_chain, 1001));
  const Bytes root = root_of(main_chain);
  const Bytes proven = take(aviary_child_proof_verify(root.data(), proof.data(), proof.size()));
  CHECK(proven == take(aviary_chain_child_record(main_chain, 1001)));
  {
    Bytes bad_root = root;
    bad_root[5] ^= 1;
    CHECK(!take_error(aviary_child_proof_verify(bad_root.data(), proof.data(), proof.size())).empty());
    Bytes bad_proof = proof;
    bad_proof[80] ^= 1;
    CHECK(!take_error(aviary_child_proof_verify(root.data(), bad_proof.data(), bad_proof.size())).empty());
    bad_proof = proof;
    bad_proof.push_back(0);
    CHECK(!take_error(aviary_child_proof_verify(root.data(), bad_proof.data(), bad_proof.size())).empty());
    const Bytes old_root = [&] {
      const Bytes h = take(aviary_block_header(block1.data(), block1.size()));
      return take(aviary_header_subchain_root(h.data(), h.size()));
    }();
    CHECK(!take_error(aviary_child_proof_verify(old_root.data(), proof.data(), proof.size())).empty());
    CHECK(!take_error(aviary_child_proof_verify(nullptr, proof.data(), proof.size())).empty());
  }
  const Bytes record_proof = take(aviary_chain_prove_record(user, key.data()));
  CHECK(aviary_verify_state_proof(parse(proven).tip_state_root.data(), record_proof.data(), record_proof.size()) == 1);
  const Bytes hop = take(aviary_chain_subchain_hop_v2(main_chain, 1001));
  CHECK(!hop.empty() && hop[0] == 2 && Bytes(hop.begin() + 1, hop.end()) == proof);
  const Bytes hops = cabi::list({hop});
  const Bytes path = take(aviary_subchain_proof_v2_encode(hops.data(), hops.size(), record_proof.data(),
                                                          record_proof.size()));
  CHECK(aviary_subchain_proof_v2_verify(root.data(), path.data(), path.size()) == 1);
  {
    Bytes bad = path;
    bad[bad.size() - 3] ^= 1;
    CHECK(aviary_subchain_proof_v2_verify(root.data(), bad.data(), bad.size()) == 0);
    Bytes bad_root = root;
    bad_root[0] ^= 1;
    CHECK(aviary_subchain_proof_v2_verify(bad_root.data(), path.data(), path.size()) == 0);
    CHECK(aviary_subchain_proof_v2_verify(root.data(), nullptr, 0) == 0);
    Bytes wrong_kind = hop;
    wrong_kind[0] = 1;
    const Bytes wrong = cabi::list({wrong_kind});
    const Bytes wrong_path = take(aviary_subchain_proof_v2_encode(wrong.data(), wrong.size(), record_proof.data(),
                                                                  record_proof.size()));
    CHECK(aviary_subchain_proof_v2_verify(root.data(), wrong_path.data(), wrong_path.size()) == 0);
    wrong_kind[0] = 7;
    const Bytes unknown = cabi::list({wrong_kind});
    CHECK(!take_error(aviary_subchain_proof_v2_encode(unknown.data(), unknown.size(), record_proof.data(),
                                                      record_proof.size()))
               .empty());
  }

  std::printf("== everything is persisted and brought back ==\n");
  const Bytes main_snapshot = take(aviary_chain_snapshot(main_chain));
  const Bytes user_snapshot = take(aviary_chain_snapshot(user));
  AviaryChain main_restored =
      aviary_chain_restore(main_genesis.data(), main_genesis.size(), 0, server.pub.data(), server.pub.size(),
                           server.sec.data(), server.sec.size(), main_snapshot.data(), main_snapshot.size());
  AviaryChain user_restored =
      aviary_chain_restore(user_genesis.data(), user_genesis.size(), 0, user_key.pub.data(), user_key.pub.size(),
                           user_key.sec.data(), user_key.sec.size(), user_snapshot.data(), user_snapshot.size());
  CHECK(main_restored != nullptr && user_restored != nullptr);
  CHECK(take(aviary_chain_snapshot(main_restored)) == main_snapshot);
  CHECK(take(aviary_chain_child_proof(main_restored, 1001)) == proof);
  CHECK(take(aviary_chain_archived_block(main_restored, 2)) == block2);
  for (AviaryChain u : {user, user_restored}) commit(u, take(aviary_chain_propose(u, 22)));
  const Bytes next_anchor = take(aviary_chain_pending_anchor(main_chain, user, 0));
  CHECK(next_anchor == take(aviary_chain_pending_anchor(main_restored, user_restored, 0)));
  const Bytes straight3 = commit(main_chain, propose(main_chain, 12, {next_anchor}, {}));
  const Bytes restored3 = commit(main_restored, propose(main_restored, 12, {next_anchor}, {}));
  CHECK(straight3 == restored3);
  CHECK(take(aviary_chain_snapshot(main_restored)) == take(aviary_chain_snapshot(main_chain)));
  CHECK(take(aviary_chain_snapshot(user_restored)) == take(aviary_chain_snapshot(user)));

  std::printf("== registrar rotation, a second user chain, and closing ==\n");
  {
    const Bytes both = cabi::list({reg0, reg1});
    const Bytes rotate = take(aviary_subchain_op_set_registrars(0, 0, 2, both.data(), both.size()));
    const Bytes rotate_signed = take(aviary_subchain_op_sign(rotate.data(), rotate.size(), 1, main_hash.data(), 0, 1,
                                                             registrar.data(), registrar.size()));
    commit(main_restored, propose(main_restored, 13, {}, {rotate_signed}));
    CHECK(aviary_chain_registry_counts(main_restored, nullptr, nullptr, &epoch) == 1 && epoch == 1);

    const Bytes g2 = [&] {
      const Bytes g = record_genesis(1002, other_key);
      return take(aviary_genesis_commit_parent_binding(g.data(), g.size(), 1, main_hash.data()));
    }();
    const Bytes h2 = take(aviary_genesis_header(g2.data(), g2.size()));
    const Bytes r2 = take(aviary_child_registration_encode(1002, h2.data(), h2.size(), other_key.pub.data(), &power, 1, 0));
    const Bytes stale = take(aviary_subchain_op_register(0, 0, r2.data(), r2.size()));
    const Bytes stale_signed = take(aviary_subchain_op_sign(stale.data(), stale.size(), 1, main_hash.data(), 0, 1,
                                                            registrar.data(), registrar.size()));
    CHECK(propose_error(main_restored, 14, {}, {stale_signed}).find("epoch") != std::string::npos);
    const Bytes fresh = take(aviary_subchain_op_register(1, 0, r2.data(), r2.size()));
    const Bytes one = take(aviary_subchain_op_sign(fresh.data(), fresh.size(), 1, main_hash.data(), 0, 1,
                                                   registrar.data(), registrar.size()));
    CHECK(propose_error(main_restored, 14, {}, {one}).find("not enough approvals") != std::string::npos);
    const Bytes fresh_digest = take(aviary_subchain_op_digest(fresh.data(), fresh.size(), 1, main_hash.data()));
    CHECK(fresh_digest.size() == 32);
    const Bytes two = take(aviary_subchain_op_sign(one.data(), one.size(), 1, main_hash.data(), 1, 1, second.data(),
                                                   second.size()));
    const Bytes swapped = take(aviary_subchain_op_sign(fresh.data(), fresh.size(), 1, main_hash.data(), 1, 1,
                                                       second.data(), second.size()));
    const Bytes swapped_two = take(aviary_subchain_op_sign(swapped.data(), swapped.size(), 1, main_hash.data(), 0, 1,
                                                           registrar.data(), registrar.size()));
    CHECK_MSG(two == swapped_two, "approvals are stored in canonical order whatever order they were added in");
    commit(main_restored, propose(main_restored, 14, {}, {two}));
    CHECK(aviary_chain_registry_counts(main_restored, &records, &open, &epoch) == 1 && records == 2 && open == 2);

    rec = parse(take(aviary_chain_child_record(main_restored, 1001)));
    Bytes wrong_tip = rec.tip_hash;
    wrong_tip[0] ^= 1;
    auto close_op = [&](const Bytes& tip_hash) {
      const Bytes c = take(aviary_subchain_op_close(1, 0, 1001, rec.tip_height, tip_hash.data()));
      const Bytes c1 = take(aviary_subchain_op_sign(c.data(), c.size(), 1, main_hash.data(), 0, 1, registrar.data(),
                                                    registrar.size()));
      return take(aviary_subchain_op_sign(c1.data(), c1.size(), 1, main_hash.data(), 1, 1, second.data(), second.size()));
    };
    CHECK(propose_error(main_restored, 15, {}, {close_op(wrong_tip)}).find("recorded tip") != std::string::npos);
    commit(main_restored, propose(main_restored, 15, {}, {close_op(rec.tip_hash)}));
    CHECK(aviary_chain_registry_counts(main_restored, &records, &open, &epoch) == 1 && records == 2 && open == 1);
    const Record closed = parse(take(aviary_chain_child_record(main_restored, 1001)));
    CHECK(closed.status == 2 && closed.closed_height == 6 && closed.tip_hash == rec.tip_hash);
    commit(user_restored, take(aviary_chain_propose(user_restored, 23)));
    CHECK(take(aviary_chain_pending_anchor(main_restored, user_restored, 0)).empty());
    const Bytes final_proof = take(aviary_chain_child_proof(main_restored, 1001));
    const Bytes final_root = root_of(main_restored);
    const Record final_record = parse(take(aviary_child_proof_verify(final_root.data(), final_proof.data(),
                                                                     final_proof.size())));
    CHECK(final_record.status == 2 && final_record.tip_height == 3);
    const Bytes after_close = take(aviary_chain_snapshot(main_restored));
    AviaryChain again =
        aviary_chain_restore(main_genesis.data(), main_genesis.size(), 0, server.pub.data(), server.pub.size(),
                             server.sec.data(), server.sec.size(), after_close.data(), after_close.size());
    CHECK(again != nullptr && take(aviary_chain_snapshot(again)) == after_close);
    aviary_chain_free(again);
  }

  CHECK(!take_error(aviary_chain_propose_subchain(nullptr, 1, nullptr, 0, nullptr, 0)).empty());
  CHECK(!take_error(aviary_chain_propose_subchain(user, 30, nullptr, 0, signed_op.data(), 3)).empty());
  CHECK(!take_error(aviary_encoded_header_hash(nullptr, 0)).empty());
  aviary_chain_free(main_restored);
  aviary_chain_free(user_restored);
  aviary_chain_free(main_chain);
  aviary_chain_free(user);
  return bctest::summary();
}
