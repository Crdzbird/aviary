#include "aviary/chain_snapshot.hpp"

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/utxo_state.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace aviary::node {

namespace {
constexpr std::uint8_t kMagic[4] = {'A', 'V', 'C', 'S'};

codec::ByteView bv(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hv(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < h.size() && i < b.size(); ++i) h[i] = b[i];
  return h;
}

codec::u128 state_fee(const StateMachine& state) {
  if (const auto* account = dynamic_cast<const AccountStateMachine*>(&state)) return account->flat_fee();
  if (const auto* utxo = dynamic_cast<const UtxoStateMachine*>(&state)) return utxo->flat_fee();
  return 0;
}

void check_archive(const BlockStore& archive, const Bytes& encoded, const BlockHeader& tip) {
  if (archive.serialize() != encoded) throw std::invalid_argument("chain snapshot: archive is not canonical");
  if (archive.empty()) return;
  if (archive.tip_height() != tip.height)
    throw std::invalid_argument("chain snapshot: archive does not end at the tip");
  Hash expected_parent{};
  for (std::uint64_t h = archive.first_height(); h <= archive.tip_height(); ++h) {
    if (!archive.has_height(h)) throw std::invalid_argument("chain snapshot: archive has a gap");
    const StoredBlock& sb = archive.by_height(h);
    const BlockHeader& header = sb.block.header;
    if (header.height != h || header.chain_id != tip.chain_id)
      throw std::invalid_argument("chain snapshot: archived block does not belong at its height");
    if (h != archive.first_height() && header.parent_hash != expected_parent)
      throw std::invalid_argument("chain snapshot: archived blocks do not link");
    if (compute_tx_root(sb.block.transactions) != header.tx_root)
      throw std::invalid_argument("chain snapshot: archived transactions do not match their header");
    if (header.version >= 3 && compute_anchor_root(sb.block.anchors) != header.anchor_root)
      throw std::invalid_argument("chain snapshot: archived anchors do not match their header");
    if (header.version >= 4 && compute_ops_root(sb.block.ops) != header.ops_root)
      throw std::invalid_argument("chain snapshot: archived registry operations do not match their header");
    expected_parent = header.hash();
    if (sb.commit.height != h || sb.commit.block_id != expected_parent)
      throw std::invalid_argument("chain snapshot: archived certificate is for another block");
  }
  if (expected_parent != tip.hash())
    throw std::invalid_argument("chain snapshot: archive tip is not the ledger tip");
}

using SetsByHash = std::map<Hash, consensus::ValidatorSet>;

/// Every full validator set the snapshot can vouch for, keyed by hash: the
/// genesis set, the engine's certifying and in force sets, and each set an
/// archived header announces. A header commits the hash of the set in force
/// at its height, so any set found here under that hash is that set.
SetsByHash known_sets(const GenesisSpec& genesis, const consensus::BftEngine& bft, const BlockStore& archive) {
  SetsByHash sets;
  const auto add = [&sets](const consensus::ValidatorSet& s) { sets.emplace(s.hash(), s); };
  add(genesis.validator_set());
  add(bft.tip_validators());
  add(bft.validators());
  for (std::uint64_t h = archive.first_height(); h <= archive.tip_height(); ++h) {
    const BlockHeader& header = archive.by_height(h).block.header;
    if (std::optional<consensus::ValidatorSet> next = consensus::announced_validator_set(header)) add(*next);
  }
  return sets;
}

consensus::ConsensusData consensus_data_of(const BlockHeader& header) {
  try {
    return consensus::ConsensusData::decode_all(bv(header.consensus_data));
  } catch (const std::exception&) {
    throw std::invalid_argument("chain snapshot: an archived block carries no BFT consensus data");
  }
}

bool embedded_by_next(const BlockStore& archive, std::uint64_t h) {
  if (h >= archive.tip_height()) return false;
  const consensus::ConsensusData next = consensus_data_of(archive.by_height(h + 1).block.header);
  return next.last_commit && next.last_commit->encode() == archive.by_height(h).commit.encode();
}

/// Verifies every archived certificate (signer list, quorum power and the
/// aggregate BLS signature) under the set its block's header commits. When
/// that set is not in the snapshot, the certificate must instead equal the
/// last commit the next archived header embeds, which the hash links to the
/// verified tip authenticate.
void check_certificates(const GenesisSpec& genesis, const consensus::BftEngine& bft, const BlockStore& archive) {
  if (archive.empty()) return;
  const SetsByHash sets = known_sets(genesis, bft, archive);
  for (std::uint64_t h = archive.first_height(); h <= archive.tip_height(); ++h) {
    const StoredBlock& sb = archive.by_height(h);
    const auto found = sets.find(consensus_data_of(sb.block.header).validator_set_hash);
    if (found != sets.end()) {
      const consensus::CommitError e = consensus::verify_commit(genesis.chain_id, found->second, sb.commit);
      if (!e.ok)
        throw std::invalid_argument("chain snapshot: archived certificate at height " + std::to_string(h) +
                                    " rejected: " + e.error);
      continue;
    }
    if (!embedded_by_next(archive, h))
      throw std::invalid_argument("chain snapshot: archived certificate at height " + std::to_string(h) +
                                  " cannot be verified: its validator set is not in the snapshot");
  }
}
}  // namespace

Bytes ChainSnapshot::encode() const {
  codec::CodecWriter w;
  for (std::uint8_t b : kMagic) w.write_u8(b);
  w.write_u8(kVersion);
  w.write_fixed_bytes(hv(genesis_hash), 32);
  w.write_bytes(bv(ledger));
  w.write_list(receipts, [](codec::CodecWriter& ww, const Receipt& r) {
    const Bytes e = r.encode();
    ww.write_bytes(codec::ByteView(e.data(), e.size()));
  });
  w.write_bytes(bv(engine));
  w.write_bool(archive.has_value());
  if (archive) w.write_bytes(bv(*archive));
  return w.take();
}

ChainSnapshot ChainSnapshot::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  for (std::uint8_t b : kMagic)
    if (r.read_u8() != b) throw codec::CodecError("chain snapshot: bad magic");
  if (r.read_u8() != kVersion) throw codec::CodecError("chain snapshot: unknown version");
  ChainSnapshot s;
  s.genesis_hash = to_hash(r.read_fixed_bytes(32));
  s.ledger = r.read_bytes();
  s.receipts = r.read_list<Receipt>([](codec::CodecReader& rr) {
    const Bytes e = rr.read_bytes();
    return Receipt::decode_all(codec::ByteView(e.data(), e.size()));
  });
  s.engine = r.read_bytes();
  if (r.read_bool()) s.archive = r.read_bytes();
  r.assert_fully_consumed();
  return s;
}

Bytes snapshot_chain(const Hash& genesis_hash, const AssembledChain& chain, const BlockStore* archive) {
  ChainSnapshot s;
  s.genesis_hash = genesis_hash;
  s.ledger = chain.engine->ledger().snapshot();
  s.receipts = chain.engine->ledger().last_receipts();
  s.engine = chain.engine->consensus_state();
  if (archive != nullptr) s.archive = archive->serialize();
  return s.encode();
}

RestoredChain restore_chain(const GenesisSpec& genesis, std::uint32_t me, crypto::KeyPair bls_key,
                            codec::ByteView snapshot, Bytes vrf_secret, std::vector<Bytes> vrf_pubkeys) {
  const ChainSnapshot s = ChainSnapshot::decode_all(snapshot);
  if (s.genesis_hash != genesis.hash())
    throw std::invalid_argument("chain snapshot: taken from a chain with another genesis");

  RestoredChain out{ChainAssembler::assemble_restored(genesis, me, std::move(bls_key), bv(s.ledger), s.receipts,
                                                      std::move(vrf_secret), std::move(vrf_pubkeys)),
                    std::nullopt};
  const Ledger& ledger = out.chain.engine->ledger();
  const BlockHeader& tip = ledger.tip();
  if (tip.chain_id != genesis.chain_id) throw std::invalid_argument("chain snapshot: tip is for another chain");
  if (ledger.state().state_root() != tip.state_root)
    throw std::invalid_argument("chain snapshot: state does not hash to the tip's state_root");
  if (ledger.snapshot() != s.ledger) throw std::invalid_argument("chain snapshot: ledger section is not canonical");
  if (state_fee(ledger.state()) != genesis.flat_fee)
    throw std::invalid_argument("chain snapshot: state carries another fee than the genesis");
  if (tip.height == 0 && ledger_from_genesis(genesis)->snapshot() != s.ledger)
    throw std::invalid_argument("chain snapshot: a height 0 ledger must be the genesis ledger");

  out.chain.engine->restore_consensus_state(bv(s.engine));
  if (out.chain.engine->consensus_state() != s.engine)
    throw std::invalid_argument("chain snapshot: engine section is not canonical");

  if (s.archive) {
    out.archive = BlockStore::load(bv(*s.archive));
    check_archive(*out.archive, *s.archive, tip);
    const auto* bft = dynamic_cast<const consensus::BftEngine*>(out.chain.engine.get());
    if (bft == nullptr) throw std::invalid_argument("chain snapshot: only a BFT chain keeps an archive");
    if (!out.archive->empty() &&
        (!bft->last_commit() ||
         out.archive->by_height(tip.height).commit.encode() != bft->last_commit()->encode()))
      throw std::invalid_argument("chain snapshot: archived tip certificate is not the engine's");
    check_certificates(genesis, *bft, *out.archive);
  }
  return out;
}

}  // namespace aviary::node
