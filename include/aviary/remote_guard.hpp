#pragma once

#include <map>
#include <string>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// Password-authenticated admin gate (ARCHITECTURE §: node operations). A running
/// node exposes privileged operations — remote key-store unlock, config changes —
/// that must be gated by an operator credential. A password sent over a channel,
/// or a password hash stored on the node, is the classic weak link: phishable in
/// flight, offline-crackable at rest.
///
/// [RemoteGuard] closes both with OPAQUE (an asymmetric PAKE): the password NEVER
/// leaves the admin's client, and the node stores NO password-equivalent — only
/// an OPAQUE `record` that reveals nothing to a stolen-disk attacker. A correct-
/// password login mutually authenticates both sides and agrees a session key; a
/// wrong password fails without the node ever learning the password.
///
/// The capability pattern: at enrolment the admin seals a secret (a key-store
/// pass, an admin token) under the OPAQUE **export_key** — a stable key the
/// password derives client-side, which the node cannot compute. The node stores
/// that opaque blob and hands it back only on a successful login; the client
/// re-derives the export_key and opens it. So the node holds an admin capability
/// it cannot itself read, releasable only by the right password.
namespace aviary::node {

class RemoteGuard {
 public:
  /// The OPAQUE transcript context (domain separation). The client must pass the
  /// same value to `crypto::opaque_client_finish`.
  static constexpr const char* kLoginContext = "aviary.remoteguard.v1";

  /// Server long-term setup. A real node passes random material; a test passes
  /// fixed seeds for reproducibility. [oprf_seed] should be 64 bytes.
  static RemoteGuard create(crypto::ByteView server_key_seed, crypto::ByteView oprf_seed);

  /// The server public key clients need to register against.
  const Bytes& server_public_key() const { return keypair_.public_key; }

  // ── Registration (server side) ──────────────────────────────────────────────
  /// Respond to a client's blinded registration [request] for [cred_id].
  Bytes registration_response(const std::string& cred_id, crypto::ByteView request) const;
  /// Store a finalized [record] + the client's [sealed_capability] (opaque to us).
  void enroll(const std::string& cred_id, Bytes record, Bytes sealed_capability);
  bool enrolled(const std::string& cred_id) const { return admins_.count(cred_id) != 0; }

  // ── Login (server side, OPAQUE-3DH) ─────────────────────────────────────────
  struct Ke2 {
    Bytes ke2;           ///< send to the client
    Bytes server_state;  ///< keep for login_finish
  };
  /// Login step 1: respond to the client's [ke1] for [cred_id]. Throws if the id
  /// is not enrolled.
  Ke2 login_respond(const std::string& cred_id, crypto::ByteView ke1) const;

  struct Granted {
    Bytes session_key;         ///< the mutually-agreed session key
    Bytes sealed_capability;   ///< the stored blob, to hand back over the session
  };
  /// Login step 2: verify the client's [ke3]. On success returns the shared
  /// session key AND the stored capability blob. Throws on auth failure (wrong
  /// password) — nothing is released.
  Granted login_finish(const std::string& cred_id, crypto::ByteView server_state,
                       crypto::ByteView ke3) const;

  // ── Capability sealing (client side helpers) ────────────────────────────────
  /// Seal [plaintext] under an OPAQUE [export_key] (committing AEAD). The node
  /// stores the result but cannot open it.
  static Bytes seal_capability(crypto::ByteView export_key, crypto::ByteView plaintext);
  /// Open a [seal_capability] blob with the same export_key. Throws on tamper /
  /// wrong key.
  static Bytes open_capability(crypto::ByteView export_key, crypto::ByteView sealed);

 private:
  RemoteGuard(crypto::KeyPair kp, Bytes oprf_seed)
      : keypair_(std::move(kp)), oprf_seed_(std::move(oprf_seed)) {}

  crypto::KeyPair keypair_;
  Bytes oprf_seed_;
  struct Admin {
    Bytes record;
    Bytes sealed_capability;
  };
  std::map<std::string, Admin> admins_;
};

}  // namespace aviary::node
