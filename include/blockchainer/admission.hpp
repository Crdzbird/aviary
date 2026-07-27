#pragma once

#include <cstdint>

#include "blockchainer/credential_gate.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// The anonymous-membership admission handshake over the wire
/// (docs/design/CREDENTIAL_GATE.md §6). A verifier challenges a connecting peer
/// with a fresh nonce; the peer answers with an unlinkable credential
/// presentation; the verifier admits or refuses. Pure and transport-agnostic —
/// each function consumes/produces wire bytes ([wire::AdmissionChallenge] /
/// [wire::AdmissionPresentation]); the caller moves them over its socket. The
/// transport (`run_tcp_bft_node`) wires these three calls into its handshake.
namespace blockchainer::p2p {

/// Verifier side: a fresh challenge. Keep [nonce] to check the reply; send
/// [wire] (an encoded [wire::AdmissionChallenge]) to the peer. The nonce is a
/// transport-layer random value (unpredictable per connection), not consensus
/// state — see [crypto::random_bytes].
struct AdmissionChallenge {
  Bytes nonce;  ///< 32 random bytes; the verifier remembers this per connection
  Bytes wire;   ///< the encoded challenge message to send
};
AdmissionChallenge make_admission_challenge(std::uint32_t epoch);

/// Prover side: answer an encoded challenge [challenge_wire] with a presentation
/// of [credential] under [policy], for the "gossip" scope. Returns the encoded
/// [wire::AdmissionPresentation] to send back. Throws on a malformed challenge or
/// a backend failure.
Bytes answer_admission_challenge(const consensus::CredentialPolicy& policy,
                                 const consensus::Credential& credential,
                                 crypto::ByteView challenge_wire);

/// Verifier side: does [presentation_wire] admit against [gate], for the [epoch]
/// and [nonce] this verifier issued and the current [now]? Runs the gossip-scope
/// admission check. Never throws — a malformed reply or failed proof is a quiet
/// false.
bool admission_granted(const consensus::CredentialGate& gate, std::uint32_t epoch,
                       crypto::ByteView nonce, crypto::ByteView presentation_wire,
                       std::uint64_t now);

}  // namespace blockchainer::p2p
