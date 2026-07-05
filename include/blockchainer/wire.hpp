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

Message make(const Hello&);
Message make(const TxMsg&);
Message make(const BlockMsg&);
Message make(const GetBlocks&);
Message make(const EvidenceMsg&);

Hello as_hello(const Message&);
TxMsg as_tx(const Message&);
BlockMsg as_block(const Message&);
GetBlocks as_get_blocks(const Message&);
EvidenceMsg as_evidence(const Message&);

}  // namespace blockchainer::wire
