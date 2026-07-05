#pragma once

#include <cstdint>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/types.hpp"

/// P2P wire protocol (ARCHITECTURE §9). Every message is canonically encoded so
/// the framing is deterministic and the decoders are fuzzable. A [Message] is a
/// type tag plus a length-prefixed payload; typed messages encode into the
/// payload.
namespace blockchainer::wire {

enum class MessageType : std::uint8_t {
  kHello = 1,      ///< peer handshake / capability advertisement
  kTx = 2,         ///< gossip a transaction
  kBlock = 3,      ///< gossip a finalized block + its commit certificate
  kGetBlocks = 4,  ///< request a range of blocks (sync)
  kEvidence = 5,   ///< gossip proof that a validator double-signed
  kConsensus = 6,  ///< a Tendermint proposal / prevote / precommit
};

struct Message {
  MessageType type = MessageType::kHello;
  Bytes payload;

  Bytes encode() const;
  static Message decode_all(codec::ByteView bytes);
};

// ── Typed messages ───────────────────────────────────────────────────────────

struct Hello {
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  std::uint64_t tip_height = 0;
};

struct TxMsg {
  Transaction tx;
};

struct BlockMsg {
  Block block;
  consensus::CommitCertificate commit;
};

struct GetBlocks {
  std::uint64_t from_height = 0;
  std::uint32_t count = 0;
};

struct EvidenceMsg {
  consensus::Evidence evidence;
};

/// A Tendermint consensus message. A proposal carries the full [block]; a
/// prevote/precommit carries a BLS [signature] over the vote. [has_value] is
/// false for a nil vote.
struct ConsensusMsg {
  std::uint8_t kind = 0;  ///< 0 = proposal, 1 = prevote, 2 = precommit
  std::uint64_t height = 0;
  std::uint32_t round = 0;
  std::uint32_t sender = 0;  ///< validator index
  bool has_value = false;
  Hash block_id{};            ///< the value (valid iff has_value)
  std::int64_t valid_round = -1;  ///< proposal only
  Bytes signature;            ///< BLS vote signature (votes only)
  Bytes block;                ///< encoded Block (proposals only)
};

Message make(const Hello&);
Message make(const TxMsg&);
Message make(const BlockMsg&);
Message make(const GetBlocks&);
Message make(const EvidenceMsg&);
Message make(const ConsensusMsg&);

Hello as_hello(const Message&);
TxMsg as_tx(const Message&);
BlockMsg as_block(const Message&);
GetBlocks as_get_blocks(const Message&);
EvidenceMsg as_evidence(const Message&);
ConsensusMsg as_consensus(const Message&);

}  // namespace blockchainer::wire
