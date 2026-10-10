#include "aviary/admission.hpp"

#include "aviary/wire.hpp"

namespace aviary::p2p {

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
}  // namespace

AdmissionChallenge make_admission_challenge(std::uint32_t epoch) {
  Bytes nonce = crypto::random_bytes(32);
  wire::AdmissionChallenge msg;
  msg.epoch = epoch;
  msg.challenge = nonce;
  return AdmissionChallenge{nonce, wire::make(msg).encode()};
}

Bytes answer_admission_challenge(const consensus::CredentialPolicy& policy,
                                 const consensus::Credential& credential,
                                 crypto::ByteView challenge_wire) {
  wire::AdmissionChallenge c = wire::as_admission_challenge(wire::Message::decode_all(challenge_wire));
  Bytes pres = consensus::CredentialGate::present(policy, credential, c.epoch, "gossip",
                                                  view(c.challenge));
  return wire::make(wire::AdmissionPresentation{pres}).encode();
}

bool admission_granted(const consensus::CredentialGate& gate, std::uint32_t epoch,
                       crypto::ByteView nonce, crypto::ByteView presentation_wire,
                       std::uint64_t now) {
  Bytes presentation;
  try {
    presentation =
        wire::as_admission_presentation(wire::Message::decode_all(presentation_wire)).presentation;
  } catch (...) {
    return false;  // malformed reply
  }
  return gate.admit(epoch, "gossip", nonce, view(presentation), now);
}

}  // namespace aviary::p2p
