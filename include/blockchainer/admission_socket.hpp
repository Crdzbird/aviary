#pragma once

#include <cstdint>

#include "blockchainer/credential_gate.hpp"

/// Blocking membership admission over a connected socket — the one-shot
/// challenge→present→admit round a non-validator gossip / submitter peer runs
/// before it joins the mesh (docs/design/CREDENTIAL_GATE.md §6). This is a
/// standalone pre-mesh handshake, deliberately separate from the consensus
/// reactor (`run_tcp_bft_node`, which mutually authenticates *validators* by
/// signing key). Messages are u32-BE length-prefixed, matching that reactor's
/// framing. Both calls are synchronous; run them on opposite ends of a connected
/// socket pair (typically one thread each).
namespace blockchainer::p2p {

/// Verifier side: challenge the peer on [fd], read its presentation, and decide.
/// Returns true iff the peer answers within [timeout_ms] with a presentation that
/// admits under [gate] for scope "gossip" at ([epoch], [now]). Any I/O error,
/// timeout, malformed reply, or failed proof is a quiet false — never throws.
bool admit_peer_over_socket(int fd, const consensus::CredentialGate& gate, std::uint32_t epoch,
                            std::uint64_t now, int timeout_ms = 5000);

/// Prover side: read the verifier's challenge on [fd] and answer with a
/// presentation of [credential] under [policy] (scope "gossip"). Returns true iff
/// the presentation was sent; the verifier's verdict is out-of-band (the mesh
/// proceeds on success or the socket is closed on refusal). Never throws.
bool present_over_socket(int fd, const consensus::CredentialPolicy& policy,
                         const consensus::Credential& credential, int timeout_ms = 5000);

}  // namespace blockchainer::p2p
