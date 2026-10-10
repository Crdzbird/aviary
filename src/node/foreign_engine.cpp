#include <memory>
#include "aviary/foreign_engine.hpp"

#include <stdexcept>

#include "aviary/chain_assembler.hpp"

namespace aviary::node {

ForeignEngine::ForeignEngine(std::unique_ptr<Ledger> ledger, JustifyFn justify, VerifyFn verify)
    : ledger_(std::move(ledger)), justify_(std::move(justify)), verify_(std::move(verify)) {
  if (!ledger_ || !justify_ || !verify_)
    throw std::runtime_error("foreign engine: ledger, justify and verify are all required");
}

Block ForeignEngine::propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
  return ledger_->build_block(std::move(txs), timestamp);
}
Block ForeignEngine::propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                                      std::vector<Anchor> anchors) {
  return ledger_->build_block(std::move(txs), timestamp, std::move(anchors));
}

BlockError ForeignEngine::submit(const Block& block, codec::ByteView justification) {
  bool ok = false;
  try {
    ok = verify_(block, justification);
  } catch (...) {
    ok = false;  // a throwing foreign verifier is a failed verification, never a crash
  }
  if (!ok) return BlockError::failure("foreign engine: justification rejected");
  return ledger_->process_block(block);
}

void register_foreign_engine(const std::string& id, JustifyFn justify, VerifyFn verify,
                             bool grind_proof) {
  ComponentRegistry::register_engine(
      id,
      [justify, verify](BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
        return std::make_unique<ForeignEngine>(std::move(c.ledger), justify, verify);
      },
      grind_proof);
}

void register_foreign_proposer(const std::string& id, ForeignProposerFn fn) {
  ComponentRegistry::register_proposer(id, [fn] {
    return consensus::ProposerPolicy(
        [fn](std::uint64_t height, const consensus::ValidatorSet& v) {
          return fn(height, static_cast<std::uint32_t>(v.size()));
        });
  });
}

}  // namespace aviary::node
