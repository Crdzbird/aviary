#include "blockchainer/wire.hpp"

namespace blockchainer::wire {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

bool valid_type(std::uint8_t t) {
  return t >= static_cast<std::uint8_t>(MessageType::kHello) &&
         t <= static_cast<std::uint8_t>(MessageType::kEvidence);
}

void expect(const Message& m, MessageType t) {
  if (m.type != t) throw codec::CodecError("wire: unexpected message type");
}
}  // namespace

Bytes Message::encode() const {
  codec::CodecWriter w;
  w.write_u8(static_cast<std::uint8_t>(type));
  w.write_bytes(view(payload));
  return w.take();
}

Message Message::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  const std::uint8_t t = r.read_u8();
  if (!valid_type(t)) throw codec::CodecError("wire: unknown message type");
  Message m;
  m.type = static_cast<MessageType>(t);
  m.payload = r.read_bytes();
  r.assert_fully_consumed();
  return m;
}

// ── Hello ────────────────────────────────────────────────────────────────────

Message make(const Hello& h) {
  codec::CodecWriter w;
  w.write_u8(h.version);
  w.write_u64(h.chain_id);
  w.write_u64(h.tip_height);
  return Message{MessageType::kHello, w.take()};
}

Hello as_hello(const Message& m) {
  expect(m, MessageType::kHello);
  codec::CodecReader r(view(m.payload));
  Hello h;
  h.version = r.read_u8();
  h.chain_id = r.read_u64();
  h.tip_height = r.read_u64();
  r.assert_fully_consumed();
  return h;
}

// ── Tx ───────────────────────────────────────────────────────────────────────

Message make(const TxMsg& t) {
  return Message{MessageType::kTx, t.tx.encode()};
}

TxMsg as_tx(const Message& m) {
  expect(m, MessageType::kTx);
  return TxMsg{Transaction::decode_all(view(m.payload))};
}

// ── Block (+ commit certificate) ─────────────────────────────────────────────

Message make(const BlockMsg& b) {
  codec::CodecWriter w;
  Bytes blk = b.block.encode();
  Bytes cert = b.commit.encode();
  w.write_bytes(view(blk));
  w.write_bytes(view(cert));
  return Message{MessageType::kBlock, w.take()};
}

BlockMsg as_block(const Message& m) {
  expect(m, MessageType::kBlock);
  codec::CodecReader r(view(m.payload));
  Bytes blk = r.read_bytes();
  Bytes cert = r.read_bytes();
  r.assert_fully_consumed();
  BlockMsg b;
  b.block = Block::decode_all(view(blk));
  b.commit = consensus::CommitCertificate::decode_all(view(cert));
  return b;
}

// ── GetBlocks ────────────────────────────────────────────────────────────────

Message make(const GetBlocks& g) {
  codec::CodecWriter w;
  w.write_u64(g.from_height);
  w.write_u32(g.count);
  return Message{MessageType::kGetBlocks, w.take()};
}

GetBlocks as_get_blocks(const Message& m) {
  expect(m, MessageType::kGetBlocks);
  codec::CodecReader r(view(m.payload));
  GetBlocks g;
  g.from_height = r.read_u64();
  g.count = r.read_u32();
  r.assert_fully_consumed();
  return g;
}

// ── Evidence (double-signing proof) ──────────────────────────────────────────

Message make(const EvidenceMsg& e) {
  return Message{MessageType::kEvidence, e.evidence.encode()};
}

EvidenceMsg as_evidence(const Message& m) {
  expect(m, MessageType::kEvidence);
  return EvidenceMsg{consensus::Evidence::decode_all(view(m.payload))};
}

}  // namespace blockchainer::wire
