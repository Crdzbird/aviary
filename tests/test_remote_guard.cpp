// RemoteGuard: a node's admin operations gated by OPAQUE. The password never
// leaves the client, the node stores no password-equivalent, and a correct
// login releases a capability the node itself cannot read.
#include <string>

#include "aviary/crypto.hpp"
#include "aviary/remote_guard.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using node::RemoteGuard;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView sv(const char* s) {
  return codec::ByteView(reinterpret_cast<const std::uint8_t*>(s), std::char_traits<char>::length(s));
}
Bytes seed(std::uint8_t b, std::size_t n) { return Bytes(n, b); }
codec::ByteView ctx() { return sv(RemoteGuard::kLoginContext); }

// Full client-side registration against [guard]: seals [capability] under the
// password-derived export_key and enrolls it. Returns nothing; the node stores
// the record + sealed capability.
void client_register(RemoteGuard& guard, const std::string& id, const char* password,
                     const Bytes& capability) {
  auto req = crypto::opaque_registration_request(sv(password));
  Bytes resp = guard.registration_response(id, view(req.request));
  auto rec = crypto::opaque_finalize_request(sv(password), view(req.blind), view(resp));
  Bytes sealed = RemoteGuard::seal_capability(view(rec.export_key), view(capability));
  guard.enroll(id, rec.record, sealed);
}
}  // namespace

int main() {
  init();
  RemoteGuard guard = RemoteGuard::create(view(seed(0x11, 32)), view(seed(0x22, 64)));
  const std::string admin = "operator@node";
  const char* password = "correct horse battery staple";
  const Bytes capability = [] {
    const char* s = "keystore-unlock-token";
    return Bytes(s, s + std::char_traits<char>::length(s));
  }();

  std::printf("== enrol an admin: the node stores a record + a sealed capability ==\n");
  client_register(guard, admin, password, capability);
  CHECK(guard.enrolled(admin));

  std::printf("== the node never holds the password or a usable capability ==\n");
  {
    // The stored record must not contain the raw password bytes, and the node
    // cannot open the sealed capability without the client's export_key.
    // (We reach into the flow: re-register into a throwaway guard to inspect.)
    RemoteGuard g2 = RemoteGuard::create(view(seed(0x33, 32)), view(seed(0x44, 64)));
    auto req = crypto::opaque_registration_request(sv(password));
    Bytes resp = g2.registration_response(admin, view(req.request));
    auto rec = crypto::opaque_finalize_request(sv(password), view(req.blind), view(resp));
    // The 192-byte record does not embed the password.
    const std::string pw(password);
    bool leaks = false;
    for (std::size_t i = 0; i + pw.size() <= rec.record.size(); ++i)
      if (std::equal(pw.begin(), pw.end(), rec.record.begin() + i)) leaks = true;
    CHECK_MSG(!leaks, "the OPAQUE record does not contain the password");
    // A wrong export_key cannot open the sealed capability.
    Bytes sealed = RemoteGuard::seal_capability(view(rec.export_key), view(capability));
    bool threw = false;
    try {
      (void)RemoteGuard::open_capability(view(seed(0x99, 64)), view(sealed));
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "the node cannot open the capability without the password-derived key");
  }

  std::printf("== correct-password login: mutual auth + capability release ==\n");
  {
    auto l1 = crypto::opaque_client_init(sv(password));
    auto ke2 = guard.login_respond(admin, view(l1.ke1));
    auto l3 = crypto::opaque_client_finish(view(l1.client_state), view(ke2.ke2), ctx());
    auto granted = guard.login_finish(admin, view(ke2.server_state), view(l3.ke3));

    CHECK_MSG(l3.session_key == granted.session_key, "client and node agree on the session key");
    CHECK(!l3.session_key.empty());
    // The client re-derives the export_key and opens the capability the node
    // handed back.
    Bytes recovered = RemoteGuard::open_capability(view(l3.export_key), view(granted.sealed_capability));
    CHECK_MSG(recovered == capability, "the right password retrieves the admin capability");
  }

  std::printf("== wrong-password login fails, releasing nothing ==\n");
  {
    auto l1 = crypto::opaque_client_init(sv("wrong password"));
    auto ke2 = guard.login_respond(admin, view(l1.ke1));
    // The client detects the server/credential mismatch and cannot finish.
    bool client_failed = false;
    try {
      (void)crypto::opaque_client_finish(view(l1.client_state), view(ke2.ke2), ctx());
    } catch (const std::exception&) {
      client_failed = true;
    }
    CHECK_MSG(client_failed, "a wrong password cannot complete the client handshake");
  }

  std::printf("== a forged KE3 fails server verification (nothing released) ==\n");
  {
    // A real (correct-password) client handshake, but the attacker tampers KE3.
    auto l1 = crypto::opaque_client_init(sv(password));
    auto ke2 = guard.login_respond(admin, view(l1.ke1));
    auto l3 = crypto::opaque_client_finish(view(l1.client_state), view(ke2.ke2), ctx());
    Bytes bad_ke3 = l3.ke3;
    bad_ke3[bad_ke3.size() / 2] ^= 0x01;
    bool threw = false;
    try {
      (void)guard.login_finish(admin, view(ke2.server_state), view(bad_ke3));
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "the node rejects a tampered KE3 and releases nothing");
  }

  std::printf("== login against an unknown credential is rejected ==\n");
  {
    auto l1 = crypto::opaque_client_init(sv(password));
    bool threw = false;
    try {
      (void)guard.login_respond("nobody@node", view(l1.ke1));
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK(threw);
  }

  return bctest::summary();
}
