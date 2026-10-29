# Performance

## Method

`tools/bench_paths.cpp` drives every path through the C ABI only, so the
same source runs against any shipped `libaviary_c`.
`scripts/android/bench_paths.sh` cross builds it with the Android NDK,
pushes it with the engine to a connected device and runs it pinned to one
core (default 7, a Cortex-X1 on Tensor G2). Each path runs next to its plain
counterpart in the same process:

- **Ledger:** 3 blocks of 1,000 Ed25519 transfers through a node (sign and
  admit, then build, validate, apply and certify), follower replay of the
  snapshot with every signature and certificate verified, and 100 hybrid
  Ed25519 + ML-DSA-65 transfers.
- **Key store against raw secrets:** 1,000 transfers signed with the secret
  in memory, then 1,000 signed from a sealed key store, plus the Argon2id
  cost of creating and unlocking a store.
- **Sealed mempool against the plain one:** 1,000 signed transfers admitted
  and finalized directly, then 1,000 sealed to the proposer's hybrid KEM key,
  admitted without decryption, opened by the proposer, admitted and
  finalized.
- **Encrypted links against plaintext:** 20 heights of a 4 validator full
  mesh of `NetNode`s, every frame passed in the clear, through Noise XX
  sessions (with identity proofs), or through SecureSession; plus the
  handshake per link and a 1 KiB frame round trip.

## Pixel 7 Pro, Android 17

Tensor G2 pinned to a Cortex-X1 core at 2.85 GHz, thermal status 0 before
every run, three runs of each configuration, medians shown. "1.1.1,
before" is the engine of commit 4f6ba2b; "1.1.1, Aviary changes" is the
engine of e00db9d on the same CipherBird; "1.3.0, as shipped" is that engine
on CipherBird 1.3.0, whose public outputs (digests, ciphertexts, signatures)
are no longer page-locked (CipherBird#6).

| Path | Work | 1.1.1, before | 1.1.1, Aviary changes | 1.3.0, as shipped |
|---|---|---|---|---|
| plain | Ed25519 transfer, sign and admit | 110.0 us | 114.4 us | 82.8 us |
| plain | BFT block of 1,000, validate, apply, certify | 223.0 us | 99.8 us | 18.8 us |
| plain | Follower replay, all signatures and certificates | 111.7 us | 114.2 us | 67.4 us |
| plain | Hybrid Ed25519 + ML-DSA-65 transfer, sign and admit | 768.1 us | 727.9 us | 735.4 us |
| key store | Create the store (Argon2id key derivation) | 283.4 ms | 283.2 ms | 283.4 ms |
| key store | Seal an account into the store | 58.9 us | 60.1 us | 33.9 us |
| raw secret | Ed25519 transfer, sign and admit | 115.6 us | 120.2 us | 85.9 us |
| key store | Ed25519 transfer, unseal, sign and admit | 152.2 us | 154.2 us | 130.2 us |
| key store | Unlock a saved store (Argon2id) | 140.7 ms | 140.4 ms | 146.0 ms |
| plain mempool | Admit a signed transaction | 89.3 us | 93.9 us | 65.3 us |
| plain mempool | Admit and finalize | 310.5 us | 193.7 us | 83.2 us |
| sealed mempool | Seal to the proposer (client side) | 205.9 us | 206.8 us | 173.9 us |
| sealed mempool | Admit sealed, no decryption | 1.0 us | 1.0 us | 1.0 us |
| sealed mempool | Proposer opens the batch | 174.1 us | 179.9 us | 180.5 us |
| sealed mempool | Seal, admit, open, admit and finalize | 684.9 us | 573.7 us | 442.3 us |
| sealed mempool | Size of a sealed transfer | 1501 bytes against 173 plain | 1501 bytes against 173 plain | 1501 bytes against 173 plain |
| plaintext links | 4 validator BFT height, full mesh | 127.3 ms | 64.1 ms | 62.8 ms |
| plaintext links | Frames and wire bytes per height | 15568 bytes | 15568 bytes | 15568 bytes |
| Noise links | Handshake per link, both sides | 2.0 ms | 2.0 ms | 1.9 ms |
| Noise links | 4 validator BFT height, full mesh | 128.0 ms | 65.0 ms | 63.5 ms |
| Noise links | Frames and wire bytes per height | 16864 bytes | 16864 bytes | 16864 bytes |
| SecureSession links | Handshake per link, both sides | 1.8 ms | 1.7 ms | 1.8 ms |
| SecureSession links | 4 validator BFT height, full mesh | 128.4 ms | 65.4 ms | 64.2 ms |
| SecureSession links | Frames and wire bytes per height | 16945 bytes | 16945 bytes | 16945 bytes |
| Noise | Encrypt and decrypt a 1 KiB frame | 16.5 us | 16.6 us | 11.8 us |
| SecureSession | Encrypt and decrypt a 1 KiB frame | 16.9 us | 16.8 us | 16.5 us |

## What changed in the engine

- **One signature check per transaction.** The mempool, block building and
  block application each verified every signature. A bounded
  `SignatureCache` (8,192 entries, about 0.7 MB), owned by the ledger and
  shared with the mempool, remembers transactions whose signature passed,
  keyed by the tx root leaf, which commits to the whole encoding. A cached
  answer is the answer verification would give, so no result can change; a
  tampered copy has a different leaf and is verified (and rejected) again.
- **No repeated consensus checks per height.** `RoundDriver::applicable`
  ran the parent certificate's aggregate BLS verification and a full dry
  run of the block every time the round state asked; the answer cannot
  change within a height, so it is memoized until the next commit.
  `aggregate_commit` re-verified precommits the driver had already verified
  on arrival; the driver now says so.
- **Dart binding.** Every call resolved its native symbol again, about
  0.6 to 2 us per call; symbols are now looked up once per library. Inputs
  go into one native block per call instead of one allocation per argument,
  and that block is wiped before it is freed, so secret keys passed in do
  not linger in freed memory. The struct used to free each result is reused
  instead of allocated per call.

Still open: CipherBird's SecretStream (behind SecureSession) still builds
its ciphertext in locked memory, so SecureSession frames did not get the
Noise frames' gain (CipherBird#6), and BFT votes are signed over index
bound messages, so a certificate costs a pairing per signer instead of two.

## Apple M3 Max, before these changes

Measured with `tools/bench_engine.cpp` (one core, Release, CipherBird 1.1.1)
on the engine of commit 4f6ba2b. To be refreshed on an idle machine.

| Work | Result |
|---|---|
| Ed25519 transfer, sign and admit to the mempool | 11,300 tx/s (88 us each) |
| BFT block of 1,000 transfers: build, validate, apply, certify | 6,300 tx/s (160 ms per block) |
| Admit and finalize, end to end | 4,000 tx/s |
| Follower replay, every certificate and signature verified | 13,400 tx/s |
| Restore a 5 block snapshot without replay | 22 ms |
| Account state proof over 1,000 accounts | under 1 us to prove, 30 us to verify, 393 bytes |
| Hybrid Ed25519 + ML-DSA-65 transfer, admit and finalize | 1,340 tx/s |
| Record writes of 64 bytes, sign and finalize | 5,900 writes/s |
| One height of a 4 validator BFT mesh, in process | 110 ms (81 gossip deliveries) |
