#include "aviary/remote_guard.hpp"

#include <stdexcept>

#include "aviary/keyring.hpp"  // custody::Vault - committing AEAD for the capability

namespace aviary::node {

namespace {
crypto::ByteView cv(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
crypto::ByteView sv(const std::string& s) {
  return crypto::ByteView(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}
crypto::ByteView context() {
  return crypto::ByteView(reinterpret_cast<const std::uint8_t*>(RemoteGuard::kLoginContext),
                          std::char_traits<char>::length(RemoteGuard::kLoginContext));
}
}  // namespace

RemoteGuard RemoteGuard::create(crypto::ByteView server_key_seed, crypto::ByteView oprf_seed) {
  crypto::KeyPair kp = crypto::oprf_derive_keypair(server_key_seed);
  return RemoteGuard(std::move(kp), Bytes(oprf_seed.begin(), oprf_seed.end()));
}

Bytes RemoteGuard::registration_response(const std::string& cred_id, crypto::ByteView request) const {
  return crypto::opaque_registration_response(request, cv(keypair_.public_key), sv(cred_id),
                                              cv(oprf_seed_));
}

void RemoteGuard::enroll(const std::string& cred_id, Bytes record, Bytes sealed_capability) {
  admins_[cred_id] = Admin{std::move(record), std::move(sealed_capability)};
}

RemoteGuard::Ke2 RemoteGuard::login_respond(const std::string& cred_id, crypto::ByteView ke1) const {
  auto it = admins_.find(cred_id);
  if (it == admins_.end()) throw std::runtime_error("remote-guard: unknown admin credential");
  crypto::OpaqueKe2 k = crypto::opaque_server_respond(context(), cv(keypair_.secret_key),
                                                      cv(keypair_.public_key), cv(it->second.record),
                                                      sv(cred_id), cv(oprf_seed_), ke1);
  return Ke2{std::move(k.ke2), std::move(k.server_state)};
}

RemoteGuard::Granted RemoteGuard::login_finish(const std::string& cred_id,
                                               crypto::ByteView server_state,
                                               crypto::ByteView ke3) const {
  auto it = admins_.find(cred_id);
  if (it == admins_.end()) throw std::runtime_error("remote-guard: unknown admin credential");
  // Throws on a wrong-password / bad-KE3 (client authentication failure) - the
  // capability is released ONLY past this line.
  Bytes session_key = crypto::opaque_server_finish(server_state, ke3);
  return Granted{std::move(session_key), it->second.sealed_capability};
}

Bytes RemoteGuard::seal_capability(crypto::ByteView export_key, crypto::ByteView plaintext) {
  // OPAQUE export_key is 64 B; the committing AEAD keys on the first 32.
  custody::Vault vault(crypto::ByteView(export_key.data(), 32));
  return vault.seal(plaintext, "aviary.remoteguard.capability");
}

Bytes RemoteGuard::open_capability(crypto::ByteView export_key, crypto::ByteView sealed) {
  custody::Vault vault(crypto::ByteView(export_key.data(), 32));
  return vault.open(Bytes(sealed.begin(), sealed.end()), "aviary.remoteguard.capability");
}

}  // namespace aviary::node
