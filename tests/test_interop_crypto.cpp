#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Bytes str(const char* s) { Bytes b; for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s)); return b; }
std::string hex(const Bytes& b) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s; for (std::uint8_t x : b) { s.push_back(k[x >> 4]); s.push_back(k[x & 0xF]); }
  return s;
}
std::string hex(const crypto::Hash& h) { return hex(Bytes(h.begin(), h.end())); }
}  // namespace

int main() {
  init();

  std::printf("== keccak-256 KATs (Ethereum) ==\n");
  CHECK(hex(crypto::keccak256(view(str("")))) ==
        "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
  CHECK(hex(crypto::keccak256(view(str("abc")))) ==
        "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");

  std::printf("== ripemd-160 KATs (Bitcoin) ==\n");
  CHECK(hex(crypto::ripemd160(view(str("")))) == "9c1185a5c5e9fc54612808977ee8f548b2258d31");
  CHECK(hex(crypto::ripemd160(view(str("abc")))) == "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc");

  std::printf("== secp256k1 pubkey == generator point for privkey=1 ==\n");
  {
    Bytes sk(32, 0);
    sk[31] = 1;
    Bytes pk = crypto::secp256k1_pubkey(view(sk), /*compressed=*/false);
    CHECK(hex(pk) ==
          "04"
          "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
          "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");
    Bytes pkc = crypto::secp256k1_pubkey(view(sk), /*compressed=*/true);
    CHECK(pkc.size() == 33);
    CHECK(hex(pkc) == "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
  }

  std::printf("== secp256k1 sign / verify / recover round-trip ==\n");
  {
    auto kp = crypto::secp256k1_keygen();
    CHECK(kp.secret_key.size() == 32);
    CHECK(kp.public_key.size() == 65);

    crypto::Hash digest = crypto::keccak256(view(str("a message to sign")));
    Bytes dg(digest.begin(), digest.end());

    Bytes sig = crypto::secp256k1_sign(view(dg), view(kp.secret_key));
    CHECK(sig.size() == 65);  // r||s||recovery_id

    Bytes rs(sig.begin(), sig.begin() + 64);
    CHECK(crypto::secp256k1_verify(view(dg), view(rs), view(kp.public_key)));

    // Wrong digest fails.
    crypto::Hash other = crypto::keccak256(view(str("different")));
    Bytes od(other.begin(), other.end());
    CHECK(!crypto::secp256k1_verify(view(od), view(rs), view(kp.public_key)));

    // Recover the signer's public key.
    Bytes recovered = crypto::secp256k1_recover(view(dg), view(sig));
    CHECK(recovered == kp.public_key);
  }

  return bctest::summary();
}
