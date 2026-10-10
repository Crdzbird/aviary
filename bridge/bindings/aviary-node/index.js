// Aviary for Node.js — koffi FFI bindings.
//
// The native library is bundled in this package under prebuilds/<platform>-<arch>/
// and loaded automatically; consumers never provide a path:
//
//   const bc = require('aviary');
//   const node = bc.newNode(1n);
//   const { secret, address } = node.newAccount();
//   node.credit(address, 1_000_000n);
//
// Aviary's native links cryptolib's C ABI; libcryptolib_c is loaded first
// (its symbols are then process-global) so libaviary_c resolves them.
//
// NOTE for publishing: ship a self-contained pair of dylibs (or statically link
// cryptolib) and rewrite the install-name to @loader_path so it runs without the
// dev build tree. During local development set AVIARY_DYLIB / CRYPTOLIB_DYLIB.

'use strict';
const path = require('node:path');
const koffi = require('koffi');
// Foreign-component callback prototypes (mirror bridge/aviary_c.h).
koffi.proto('int AviaryJustifyFn(void* ctx, const uint8_t* block, size_t block_len, uint8_t* out, size_t out_cap, size_t* out_len)');
koffi.proto('int AviaryVerifyFn(void* ctx, const uint8_t* block, size_t block_len, const uint8_t* justification, size_t just_len)');
koffi.proto('uint32_t AviaryProposerFn(void* ctx, uint64_t height, uint32_t validator_count)');
koffi.proto('int AviaryTxSourceFn(void* ctx, uint64_t chain_id, uint64_t ts, uint8_t* out, size_t out_cap, size_t* out_len)');
koffi.proto('void AviaryFinalizedFn(void* ctx, uint64_t chain_id, const uint8_t* block, size_t block_len)');
koffi.proto('void AviaryEventFn(void* ctx, const uint8_t* json, size_t len)');

// ── Resolve the bundled native libraries for this platform/arch ──────────────
function ext() {
  return process.platform === 'darwin' ? 'dylib' : process.platform === 'win32' ? 'dll' : 'so';
}
function triple() { return `${process.platform}-${process.arch}`; }
function resolveAviary() {
  return process.env.AVIARY_DYLIB ||
    path.join(__dirname, 'prebuilds', triple(), `libaviary_c.${ext()}`);
}
function resolveCryptolib() {
  // Optional: load cryptolib first so its symbols are present in the process.
  return process.env.CRYPTOLIB_DYLIB ||
    path.join(__dirname, 'prebuilds', triple(), `libcryptolib_c.${ext()}`);
}

// ── Lazy native binding ──────────────────────────────────────────────────────
// The native library loads on FIRST USE (or early via preload()), never at
// require() time — importing this module is cheap and never blocks the event
// loop. All public methods are synchronous; the first to run triggers the
// (memoized) load. No `await` is ever required to call the ledger API.
let _native = null;
function ensureLoaded() {
  if (_native) return _native;

  // Load cryptolib first (best-effort) so libaviary_c's cryptolib_*
  // imports resolve. dlopen is process-global.
  try { koffi.load(resolveCryptolib()); } catch { /* may already be linked via rpath */ }
  const lib = koffi.load(resolveAviary());

  const Buf = koffi.struct('AviaryBuffer', { data: 'void *', len: 'size_t' });
  koffi.struct('AviaryBufferResult', { buf: Buf, error: 'void *' });

  const f = (sig) => lib.func(sig);
  _native = {
    init: f('int aviary_init()'),
    version: f('const char *aviary_version()'),
    backend: f('const char *aviary_crypto_backend_version()'),
    hash: f('AviaryBufferResult aviary_hash(uint8_t*, size_t)'),
    verifyState: f('int aviary_verify_state_proof(uint8_t*, uint8_t*, size_t)'),
    verifyTx: f('int aviary_verify_tx_inclusion(uint8_t*, uint8_t*, size_t)'),
    // Node
    nodeNew: f('void *aviary_node_new(uint64_t)'),
    nodeFree: f('void aviary_node_free(void*)'),
    newAccount: f('AviaryBufferResult aviary_node_new_account(void*)'),
    accountAddress: f('AviaryBufferResult aviary_account_address(uint8_t*, size_t)'),
    credit: f('int aviary_node_credit(void*, uint8_t*, uint64_t)'),
    submit: f('AviaryBufferResult aviary_node_submit_transfer(void*, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t)'),
    submitFee: f('AviaryBufferResult aviary_node_submit_transfer_fee(void*, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t, uint64_t)'),
    ksNew: f('void *aviary_keystore_new(const char*)'),
    ksAdd: f('int aviary_keystore_add_account(void*, uint8_t*, size_t, uint8_t*, size_t, uint8_t*)'),
    ksReveal: f('AviaryBufferResult aviary_keystore_reveal(void*, uint8_t*)'),
    ksSerialise: f('AviaryBufferResult aviary_keystore_serialise(void*)'),
    ksLoad: f('void *aviary_keystore_load(uint8_t*, size_t)'),
    ksUnlock: f('int aviary_keystore_unlock(void*, const char*)'),
    ksUnlocked: f('int aviary_keystore_unlocked(void*)'),
    ksFree: f('void aviary_keystore_free(void*)'),
    kemKeygen: f('AviaryBufferResult aviary_hybrid_kem_keygen()'),
    pSealPass: f('AviaryBufferResult aviary_portable_seal_passphrase(uint8_t*, size_t, uint8_t*, size_t, const char*, uint64_t, size_t)'),
    pOpenPass: f('AviaryBufferResult aviary_portable_open_passphrase(uint8_t*, size_t, const char*)'),
    pSealTo: f('AviaryBufferResult aviary_portable_seal_to(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    pOpenTo: f('AviaryBufferResult aviary_portable_open_to(uint8_t*, size_t, uint8_t*, size_t)'),
    pSealSigned: f('AviaryBufferResult aviary_portable_seal_signed_to(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    pOpenSigned: f('AviaryBufferResult aviary_portable_open_signed_from(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    thCreate: f('AviaryBufferResult aviary_threshold_create(uint16_t, uint16_t)'),
    thShareInfo: f('AviaryBufferResult aviary_threshold_share_info(uint8_t*, size_t)'),
    thShareSeal: f('AviaryBufferResult aviary_threshold_share_seal(uint8_t*, size_t, const char*, uint64_t, size_t)'),
    thShareUnseal: f('AviaryBufferResult aviary_threshold_share_unseal(uint8_t*, size_t, const char*)'),
    thCommit: f('AviaryBufferResult aviary_threshold_commit(uint8_t*, size_t)'),
    thCeremonyNew: f('void *aviary_threshold_ceremony_new(uint8_t*, uint8_t*, size_t, uint8_t*, size_t)'),
    thCeremonyFree: f('void aviary_threshold_ceremony_free(void*)'),
    thSign: f('AviaryBufferResult aviary_threshold_sign(void*, uint8_t*, size_t, uint8_t*)'),
    thVerifyShare: f('int aviary_threshold_verify_share(void*, uint16_t, uint8_t*, uint8_t*)'),
    thAggregate: f('AviaryBufferResult aviary_threshold_aggregate(void*, uint16_t*, uint8_t*, size_t)'),
    asDescribe: f('AviaryBufferResult aviary_assurance_describe(uint16_t)'),
    txEncode: f('AviaryBufferResult aviary_tx_encode(uint8_t, uint64_t, uint8_t, uint64_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    smSeal: f('AviaryBufferResult aviary_sealed_tx_seal(uint8_t*, size_t, uint8_t*, size_t)'),
    smNew: f('void *aviary_sealed_mempool_new(size_t)'),
    smAdd: f('int aviary_sealed_mempool_add(void*, uint8_t*, size_t)'),
    smSize: f('size_t aviary_sealed_mempool_size(void*)'),
    smReveal: f('AviaryBufferResult aviary_sealed_mempool_reveal(void*, uint8_t*, size_t, size_t)'),
    smFree: f('void aviary_sealed_mempool_free(void*)'),
    // CredentialGate (anonymous membership admission, Option A)
    bbsKeygen: f('AviaryBufferResult aviary_bbs_keygen(uint8_t*, size_t)'),
    credPolicyEncode: f('AviaryBufferResult aviary_credential_policy_encode(uint64_t, uint8_t*, size_t, uint8_t*, size_t, uint32_t, uint8_t)'),
    credPolicyHash: f('AviaryBufferResult aviary_credential_policy_hash(uint8_t*, size_t)'),
    credIssue: f('AviaryBufferResult aviary_credential_issue(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t, uint64_t, uint32_t)'),
    credPresent: f('AviaryBufferResult aviary_credential_present(uint8_t*, size_t, uint8_t*, size_t, uint32_t, const char*, uint8_t*, size_t)'),
    credGateNew: f('void *aviary_credential_gate_new(uint8_t*, size_t)'),
    credGateRevoke: f('void aviary_credential_gate_revoke_epoch(void*, uint32_t)'),
    credGateAdmit: f('int aviary_credential_gate_admit(void*, uint32_t, const char*, uint8_t*, size_t, uint8_t*, size_t, uint64_t)'),
    credGateFree: f('void aviary_credential_gate_free(void*)'),
    // CredentialGate Option B (pseudonymous)
    credPolicyEncodeEx: f('AviaryBufferResult aviary_credential_policy_encode_ex(uint64_t, uint8_t*, size_t, uint8_t*, size_t, uint32_t, uint8_t, uint8_t)'),
    membershipCommit: f('AviaryBufferResult aviary_membership_commit(uint8_t*, size_t, uint8_t*, size_t)'),
    blindIssue: f('AviaryBufferResult aviary_blind_issue(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t, uint64_t, uint32_t)'),
    finalizeMembership: f('AviaryBufferResult aviary_finalize_membership(uint8_t*, size_t, uint8_t*, size_t)'),
    credPresentPseud: f('AviaryBufferResult aviary_credential_present_pseudonymous(uint8_t*, size_t, uint8_t*, size_t, uint32_t, const char*, uint8_t*, size_t)'),
    credGateAdmitPseud: f('AviaryBufferResult aviary_credential_gate_admit_pseudonymous(void*, uint32_t, const char*, uint8_t*, size_t, uint8_t*, size_t, uint64_t)'),
    credGateRevokeNym: f('void aviary_credential_gate_revoke_nym(void*, uint8_t*, size_t)'),
    credGateNoteAdmission: f('int aviary_credential_gate_note_admission(void*, uint32_t, const char*, uint8_t*, size_t)'),
    // Composable chains (blueprint + registry + assembly)
    blsKeygen: f('AviaryBufferResult aviary_bls_keygen(uint8_t*, size_t)'),
    bpEncode: f('AviaryBufferResult aviary_blueprint_encode(const char*, const char*, const char*, uint8_t*, size_t)'),
    bpHash: f('AviaryBufferResult aviary_blueprint_hash(uint8_t*, size_t)'),
    registryIds: f('AviaryBufferResult aviary_registry_ids(uint8_t)'),
    genesisBuild: f('AviaryBufferResult aviary_genesis_build(uint64_t, uint64_t, uint16_t, uint8_t, uint8_t*, uint64_t*, size_t, uint8_t*, uint64_t*, size_t)'),
    genesisCommitBp: f('AviaryBufferResult aviary_genesis_commit_blueprint(uint8_t*, size_t, uint8_t*, size_t)'),
    genesisHash: f('AviaryBufferResult aviary_genesis_hash(uint8_t*, size_t)'),
    chainAssemble: f('void *aviary_chain_assemble(uint8_t*, size_t, uint32_t, uint8_t*, size_t, uint8_t*, size_t)'),
    chainHeight: f('uint64_t aviary_chain_height(void*)'),
    chainBlueprint: f('AviaryBufferResult aviary_chain_blueprint(void*)'),
    chainPropose: f('AviaryBufferResult aviary_chain_propose(void*, uint64_t)'),
    chainSeal: f('AviaryBufferResult aviary_chain_seal(void*, uint8_t*, size_t)'),
    chainMine: f('AviaryBufferResult aviary_chain_mine(void*, uint8_t*, size_t)'),
    chainSubmit: f('int aviary_chain_submit(void*, uint8_t*, size_t, uint8_t*, size_t)'),
    chainFree: f('void aviary_chain_free(void*)'),
    // Subchains (a parent block anchors a child chain's finalized headers)
    genesisHeader: f('AviaryBufferResult aviary_genesis_header(uint8_t*, size_t)'),
    childRegEncode: f('AviaryBufferResult aviary_child_registration_encode(uint64_t, uint8_t*, size_t, uint8_t*, uint64_t*, size_t, uint8_t)'),
    subchainPolicyEncode: f('AviaryBufferResult aviary_subchain_policy_encode(uint16_t, uint8_t, uint16_t, uint16_t, uint8_t*, size_t)'),
    genesisCommitSubchains: f('AviaryBufferResult aviary_genesis_commit_subchain_policy(uint8_t*, size_t, uint8_t*, size_t)'),
    genesisCommitBinding: f('AviaryBufferResult aviary_genesis_commit_parent_binding(uint8_t*, size_t, uint64_t, uint8_t*)'),
    blockHeader: f('AviaryBufferResult aviary_block_header(uint8_t*, size_t)'),
    headerSubchainRoot: f('AviaryBufferResult aviary_header_subchain_root(uint8_t*, size_t)'),
    certifiedHeaderEncode: f('AviaryBufferResult aviary_certified_header_encode(uint8_t*, size_t, uint8_t*, size_t)'),
    anchorEncode: f('AviaryBufferResult aviary_anchor_encode(uint64_t, uint8_t*, size_t)'),
    chainProposeAnchored: f('AviaryBufferResult aviary_chain_propose_anchored(void*, uint64_t, uint8_t*, size_t)'),
    chainCertify: f('AviaryBufferResult aviary_chain_certify(void*, uint8_t*, size_t)'),
    chainChildTip: f('AviaryBufferResult aviary_chain_child_tip(void*, uint64_t)'),
    chainScheduleValidators: f('int aviary_chain_schedule_validators(void*, uint8_t*, uint64_t*, size_t)'),
    lcNew: f('void *aviary_light_client_new(uint64_t, uint8_t*, size_t, uint8_t*, size_t)'),
    lcAccept: f('int aviary_light_client_accept(void*, uint8_t*, size_t, uint8_t*, size_t)'),
    lcAcceptSkipping: f('int aviary_light_client_accept_skipping(void*, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    lcHeight: f('uint64_t aviary_light_client_height(void*)'),
    lcTip: f('AviaryBufferResult aviary_light_client_tip(void*)'),
    lcValidators: f('AviaryBufferResult aviary_light_client_validators(void*)'),
    lcSetTrust: f('void aviary_light_client_set_trust_level(void*, uint32_t, uint32_t)'),
    lcSetMaxSkip: f('void aviary_light_client_set_max_skip(void*, uint64_t)'),
    lcFree: f('void aviary_light_client_free(void*)'),
    treeNodeEncode: f('AviaryBufferResult aviary_chain_tree_node_encode(uint64_t, uint8_t*, size_t, uint16_t, uint8_t*, uint64_t*, size_t, uint8_t*, uint8_t*, uint64_t*, size_t, uint16_t, uint16_t, uint8_t*, size_t)'),
    treeSpecError: f('AviaryBufferResult aviary_chain_tree_spec_error(uint8_t*, size_t)'),
    treeBuild: f('void *aviary_chain_tree_build(uint8_t*, size_t)'),
    treeChainIds: f('AviaryBufferResult aviary_chain_tree_chain_ids(void*)'),
    treeHeight: f('uint64_t aviary_chain_tree_height(void*, uint64_t)'),
    treeGenesis: f('AviaryBufferResult aviary_chain_tree_genesis(void*, uint64_t)'),
    treeTip: f('AviaryBufferResult aviary_chain_tree_tip(void*, uint64_t)'),
    treePending: f('uint32_t aviary_chain_tree_pending_anchors(void*, uint64_t)'),
    treeProduce: f('AviaryBufferResult aviary_chain_tree_produce(void*, uint64_t, uint64_t)'),
    treeProve: f('AviaryBufferResult aviary_chain_tree_prove_account(void*, uint64_t, uint8_t*)'),
    treeVerify: f('int aviary_chain_tree_verify(void*, uint8_t*, size_t)'),
    treeSetTxSource: f('void aviary_chain_tree_set_tx_source(void*, AviaryTxSourceFn*, void*)'),
    treeSetFinalized: f('void aviary_chain_tree_set_finalized_hook(void*, AviaryFinalizedFn*, void*)'),
    treeFree: f('void aviary_chain_tree_free(void*)'),
    chainValidators: f('AviaryBufferResult aviary_chain_validators(void*)'),
    chainSubchainHop: f('AviaryBufferResult aviary_chain_subchain_hop(void*, uint64_t)'),
    chainProveAccount: f('AviaryBufferResult aviary_chain_prove_account(void*, uint8_t*)'),
    subchainProofEncode: f('AviaryBufferResult aviary_subchain_proof_encode(uint8_t*, size_t, uint8_t*, size_t)'),
    subchainProofVerify: f('int aviary_subchain_proof_verify(uint8_t*, uint8_t*, size_t)'),
    // Foreign components (engines / proposers implemented in JavaScript)
    regAddEngine: f('int aviary_registry_add_engine(const char*, AviaryJustifyFn*, AviaryVerifyFn*, void*, int)'),
    regAddProposer: f('int aviary_registry_add_proposer(const char*, AviaryProposerFn*, void*)'),
    newAccountHybrid: f('AviaryBufferResult aviary_node_new_account_hybrid(void*)'),
    submitHybrid: f('AviaryBufferResult aviary_node_submit_transfer_hybrid(void*, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t)'),
    produce: f('int aviary_node_produce_block(void*, uint64_t, _Out_ uint64_t *)'),
    height: f('uint64_t aviary_node_height(void*)'),
    balance: f('uint64_t aviary_node_balance(void*, uint8_t*)'),
    nonce: f('uint64_t aviary_node_nonce(void*, uint8_t*)'),
    mempool: f('uint64_t aviary_node_mempool_size(void*)'),
    stateRoot: f('AviaryBufferResult aviary_node_state_root(void*)'),
    drainEvents: f('AviaryBufferResult aviary_node_drain_events(void*)'),
    metrics: f('AviaryBufferResult aviary_node_metrics(void*)'),
    proveAccount: f('AviaryBufferResult aviary_node_prove_account(void*, uint8_t*)'),
    // Interop
    ethAddr: f('AviaryBufferResult aviary_eth_address(uint8_t*)'),
    btcAddr: f('AviaryBufferResult aviary_btc_address(uint8_t*)'),
    solAddr: f('AviaryBufferResult aviary_sol_address(uint8_t*)'),
    erc20: f('AviaryBufferResult aviary_eth_erc20_transfer(uint8_t*, uint64_t)'),
    selector: f('AviaryBufferResult aviary_eth_selector(const char*)'),
    signLegacy: f('AviaryBufferResult aviary_eth_sign_legacy(uint64_t, uint64_t, uint64_t, uint8_t*, int, uint64_t, uint8_t*, size_t, uint64_t, uint8_t*)'),
    // Events (push), persistence, custody signing
    nodeSetEventHook: f('void aviary_node_set_event_hook(void*, AviaryEventFn*, void*)'),
    nodeSnapshot: f('AviaryBufferResult aviary_node_snapshot(void*)'),
    nodeRestore: f('void *aviary_node_restore(uint8_t*, size_t)'),
    nodeBlockAt: f('AviaryBufferResult aviary_node_block_at(void*, uint64_t)'),
    nodeSubmitTx: f('AviaryBufferResult aviary_node_submit_tx(void*, uint8_t*, size_t)'),
    nodeSubmitKs: f('AviaryBufferResult aviary_node_submit_transfer_keystore(void*, void*, uint8_t*, uint8_t*, uint64_t, uint64_t, uint64_t)'),
    ksSign: f('AviaryBufferResult aviary_keystore_sign_transfer(void*, uint64_t, uint8_t*, uint8_t*, uint64_t, uint64_t, uint64_t)'),
    ksPublicKey: f('AviaryBufferResult aviary_keystore_public_key(void*, uint8_t*)'),
    ksHas: f('int aviary_keystore_has(void*, uint8_t*)'),
    ksAddresses: f('AviaryBufferResult aviary_keystore_addresses(void*)'),
    ksLock: f('void aviary_keystore_lock(void*)'),
    // Networking: NetNode (consensus over gossip, app owned sockets) + SecureSession
    netNew: f('void *aviary_net_node_new(uint8_t*, size_t, uint32_t, uint8_t*, size_t, uint8_t*, size_t, uint64_t)'),
    netAddPeer: f('void aviary_net_node_add_peer(void*, uint32_t)'),
    netSetTxs: f('int aviary_net_node_set_transactions(void*, uint8_t*, size_t)'),
    netStart: f('AviaryBufferResult aviary_net_node_start(void*)'),
    netOnGossip: f('AviaryBufferResult aviary_net_node_on_gossip(void*, uint32_t, uint8_t*, size_t)'),
    netOnTimeout: f('AviaryBufferResult aviary_net_node_on_timeout(void*, uint8_t, uint32_t)'),
    netAdvance: f('AviaryBufferResult aviary_net_node_advance(void*)'),
    netCommitted: f('int aviary_net_node_committed(void*)'),
    netHeight: f('uint64_t aviary_net_node_height(void*)'),
    netPeerBanned: f('int aviary_net_node_peer_banned(void*, uint32_t)'),
    netReplayLog: f('AviaryBufferResult aviary_net_node_replay_log(void*)'),
    netTip: f('AviaryBufferResult aviary_net_node_tip(void*)'),
    netStateRoot: f('AviaryBufferResult aviary_net_node_state_root(void*)'),
    netBalance: f('uint64_t aviary_net_node_balance(void*, uint8_t*)'),
    netProve: f('AviaryBufferResult aviary_net_node_prove_account(void*, uint8_t*)'),
    netFree: f('void aviary_net_node_free(void*)'),
    secIdentity: f('AviaryBufferResult aviary_secure_identity_generate()'),
    secInitiate: f('void *aviary_secure_initiate(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    secAccept: f('void *aviary_secure_accept(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    secHandshake: f('AviaryBufferResult aviary_secure_handshake_message(void*)'),
    secComplete: f('int aviary_secure_complete(void*, uint8_t*, size_t)'),
    secOpen: f('int aviary_secure_open(void*)'),
    secPeer: f('AviaryBufferResult aviary_secure_peer_identity(void*)'),
    secEncrypt: f('AviaryBufferResult aviary_secure_encrypt(void*, uint8_t*, size_t)'),
    secDecrypt: f('AviaryBufferResult aviary_secure_decrypt(void*, uint8_t*, size_t)'),
    secFree: f('void aviary_secure_free(void*)'),
    // Frees
    bufFree: f('void aviary_buffer_free(AviaryBuffer*)'),
    strFree: f('void aviary_str_free(void*)'),
  };
  return _native;
}
const fn = new Proxy({}, { get: (_t, p) => ensureLoaded()[p] });

// ── Helpers ──────────────────────────────────────────────────────────────────
const u8 = (x) => Buffer.isBuffer(x) ? x : Buffer.from(x);
// Unpack the C ABI account blob [pub_len:4 BE][public][secret].
const splitBlob = (blob) => {
  const pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
  return { public: blob.subarray(4, 4 + pl), secret: blob.subarray(4 + pl) };
};
const bytes = (cb) => (!cb.data || Number(cb.len) === 0)
  ? Buffer.alloc(0) : Buffer.from(koffi.decode(cb.data, 'uint8_t', Number(cb.len)));
function consume(r, op) {
  if ((!r.buf.data || Number(r.buf.len) === 0) && r.error) {
    // Read the NUL-terminated C string the error pointer holds (koffi: length -1
    // decodes to the terminator). Decoding it as 'char *' instead reads a pointer
    // AT the address — a double-indirection that segfaults on the error path.
    const m = koffi.decode(r.error, 'char', -1); fn.strFree(r.error); throw new Error(m);
  }
  if (!r.buf.data) throw new Error(`${op || 'call'} failed`);
  const out = bytes(r.buf); fn.bufFree(r.buf); return out;
}

// Internal: synchronously load + initialize the native lib. Used by the worker
// in preload() and reachable from the main thread for the lazy fallback.
function _warm() {
  const a = ensureLoaded();
  if (a.init() !== 0) throw new Error('aviary init failed');
  a.version();
  return true;
}

// Optionally warm the native binding OFF the main thread.
//
// Spawns a short-lived worker_thread that loads the shared libraries and runs
// the one-time crypto-backend init. dlopen mapping + backend init are
// process-global, so this warms the path for the main thread's later lazy load.
// Returns a Promise<boolean> you may ignore (fire-and-forget) or await; the
// ledger API never requires awaiting it. Falls back to a same-thread warm.
function preload() {
  return new Promise((resolve) => {
    let settled = false;
    const done = (v) => { if (!settled) { settled = true; resolve(v); } };
    let Worker;
    try { ({ Worker } = require('node:worker_threads')); }
    catch { try { _warm(); } catch { /* ignore */ } return done(true); }
    try {
      const code = `
        const { workerData, parentPort } = require('node:worker_threads');
        try { require(workerData.modulePath)._warm(); parentPort.postMessage({ ok: true }); }
        catch (e) { parentPort.postMessage({ ok: false, error: String(e) }); }`;
      const w = new Worker(code, { eval: true, workerData: { modulePath: __filename } });
      w.once('message', (m) => { w.terminate(); done(!!(m && m.ok)); });
      w.once('error', () => done(false));
      w.once('exit', (c) => done(c === 0));
    } catch { done(false); }
  });
}

// ── A live in-memory chain node (ledger + BFT + mempool + observability) ──────
class Node {
  constructor(chainId) {
    this._h = fn.nodeNew(BigInt(chainId));
    if (!this._h) throw new Error('node creation failed');
  }
  free() {
    if (this._h) { if (this._event) { fn.nodeSetEventHook(this._h, null, null); this._event = null; } fn.nodeFree(this._h); this._h = null; }
  }
  static _wrap(h) { const n = Object.create(Node.prototype); n._h = h; return n; }
  /** The whole chain as one blob: chain id, genesis credits, every finalized block with its certificate. Pending txs excluded. */
  snapshot() { return consume(fn.nodeSnapshot(this._h), 'node_snapshot'); }
  /** The encoded finalized block at height (1 based). */
  blockAt(height) { return consume(fn.nodeBlockAt(this._h, BigInt(height)), 'node_block_at'); }
  /** Admit an already signed, encoded transaction; returns its txid. */
  submitTx(encoded) { return consume(fn.nodeSubmitTx(this._h, u8(encoded), u8(encoded).length), 'node_submit_tx'); }
  /** Sign a transfer from the key store account at from and admit it; the secret never leaves the engine. */
  submitTransferFromStore(store, from, to, amount, nonce, fee = 0) {
    return consume(fn.nodeSubmitKs(this._h, store._h, u8(from), u8(to), BigInt(amount), BigInt(nonce), BigInt(fee)), 'node_submit_transfer_keystore');
  }
  /** cb(json) for every chain event, synchronously from the node call that caused it; null removes the hook. While set, drainEvents() is empty. */
  onEvent(cb) {
    if (!cb) { fn.nodeSetEventHook(this._h, null, null); this._event = null; return; }
    const reg = koffi.register((_ctx, json, len) => { try { cb(Buffer.from(koffi.decode(json, 'uint8_t', Number(len))).toString('utf8')); } catch (_) {} }, koffi.pointer('AviaryEventFn'));
    this._event = reg; fn.nodeSetEventHook(this._h, reg, null);
  }

  // Fresh Ed25519 account: 64-byte secret (public = secret[32:64]), 32-byte addr.
  newAccount() {
    const secret = consume(fn.newAccount(this._h), 'new_account');
    const publicKey = secret.subarray(32, 64);
    const address = Node.accountAddress(publicKey);
    return { secret, publicKey, address };
  }
  static accountAddress(publicKey) {
    return consume(ensureLoaded().accountAddress(u8(publicKey), u8(publicKey).length), 'account_address');
  }

  // Valid until the first block is produced (reads do NOT seal genesis);
  // afterwards throws — never a silent no-op.
  credit(address, amount) {
    const rc = fn.credit(this._h, u8(address), BigInt(amount));
    if (rc === 2) throw new Error('genesis is sealed (a block was produced); fund accounts with transfers');
    if (rc !== 0) throw new Error(`credit failed (status ${rc})`);
  }
  submitTransfer(secret, toAddress, amount, nonce) {
    return consume(fn.submit(this._h, u8(secret), u8(secret).length, u8(toAddress), BigInt(amount), BigInt(nonce)), 'submit_transfer');
  }
  // Ed25519 transfer carrying a priority-fee tip (version-2 envelope) for fee-market priority.
  submitTransferFee(secret, toAddress, amount, nonce, fee) {
    return consume(fn.submitFee(this._h, u8(secret), u8(secret).length, u8(toAddress), BigInt(amount), BigInt(nonce), BigInt(fee)), 'submit_transfer_fee');
  }
  // Hybrid (Ed25519 + ML-DSA-65) post-quantum account. Returns {secret, publicKey, address}.
  newAccountHybrid() {
    const blob = consume(fn.newAccountHybrid(this._h), 'new_account_hybrid');
    // [pub_len: 4 bytes big-endian][public][secret]
    const pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
    const publicKey = blob.subarray(4, 4 + pl);
    const secret = blob.subarray(4 + pl);
    return { secret, publicKey, address: Node.accountAddress(publicKey) };
  }
  submitTransferHybrid(secret, publicKey, toAddress, amount, nonce) {
    return consume(fn.submitHybrid(this._h, u8(secret), u8(secret).length,
      u8(publicKey), u8(publicKey).length, u8(toAddress), BigInt(amount), BigInt(nonce)), 'submit_transfer_hybrid');
  }
  produceBlock(timestamp) {
    const out = [0n];
    if (fn.produce(this._h, BigInt(timestamp), out) !== 0) throw new Error('produce_block failed');
    return Number(out[0]);
  }
  height() { return Number(fn.height(this._h)); }
  mempoolSize() { return Number(fn.mempool(this._h)); }
  balance(address) { return Number(fn.balance(this._h, u8(address))); }
  nonce(address) { return Number(fn.nonce(this._h, u8(address))); }
  stateRoot() { return consume(fn.stateRoot(this._h), 'state_root'); }
  drainEvents() { return consume(fn.drainEvents(this._h), 'drain_events').toString('utf8'); }
  metrics() { return consume(fn.metrics(this._h), 'metrics').toString('utf8'); }
  proveAccount(address) { return consume(fn.proveAccount(this._h, u8(address)), 'prove_account'); }
}

// A custodial store that seals account signing secrets at rest.
class KeyStore {
  constructor(passphrase) {
    this._h = fn.ksNew(passphrase);
    if (!this._h) throw new Error('keystore create failed');
  }
  static _wrap(h) { const k = Object.create(KeyStore.prototype); k._h = h; return k; }
  static load(blob) {
    const b = u8(blob);
    const h = fn.ksLoad(b, b.length);
    if (!h) throw new Error('keystore load failed');
    return KeyStore._wrap(h);  // LOCKED — call unlock()
  }
  free() { if (this._h) { fn.ksFree(this._h); this._h = null; } }
  addAccount(publicKey, secret) {
    const pk = u8(publicKey), sk = u8(secret), out = Buffer.alloc(32);
    if (fn.ksAdd(this._h, pk, pk.length, sk, sk.length, out) !== 0) {
      throw new Error('keystore add account failed (locked?)');
    }
    return out;
  }
  reveal(address) { return consume(fn.ksReveal(this._h, u8(address)), 'keystore_reveal'); }
  serialise() { return consume(fn.ksSerialise(this._h), 'keystore_serialise'); }
  unlock(passphrase) { if (fn.ksUnlock(this._h, passphrase) !== 0) throw new Error('keystore unlock failed'); }
  get unlocked() { return fn.ksUnlocked(this._h) === 1; }
  /** Sign a transfer from the stored account without revealing its secret. Returns the encoded transaction. */
  signTransfer({ chainId, from, to, amount, nonce, fee = 0 }) {
    return consume(fn.ksSign(this._h, BigInt(chainId), u8(from), u8(to), BigInt(amount), BigInt(nonce), BigInt(fee)), 'keystore_sign_transfer');
  }
  publicKey(address) { return consume(fn.ksPublicKey(this._h, u8(address)), 'keystore_public_key'); }
  has(address) { return fn.ksHas(this._h, u8(address)) === 1; }
  addresses() { return splitBlobList(consume(fn.ksAddresses(this._h), 'keystore_addresses')); }
  /** Forget the master key until the next unlock(). */
  lock() { fn.ksLock(this._h); }
}

// ── Public API ───────────────────────────────────────────────────────────────
// An encrypted mempool (opaque handle). Submissions stay sealed to the proposer.
class SealedMempool {
  constructor(capacity = 4096) { this._h = fn.smNew(capacity); }
  add(sealedTx) { return fn.smAdd(this._h, u8(sealedTx), u8(sealedTx).length) === 1; }
  get size() { return Number(fn.smSize(this._h)); }
  reveal(proposerKemSecret, max) {
    const blob = consume(fn.smReveal(this._h, u8(proposerKemSecret), u8(proposerKemSecret).length, max), 'sealed_mempool_reveal');
    const count = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
    const txs = [];
    let off = 4;
    for (let i = 0; i < count; i++) {
      const len = (blob[off] << 24) | (blob[off+1] << 16) | (blob[off+2] << 8) | blob[off+3];
      off += 4;
      txs.push(Buffer.from(blob.subarray(off, off + len)));
      off += len;
    }
    return txs;
  }
  free() { fn.smFree(this._h); }
}

// A two-round FROST signing ceremony (opaque handle). Free() when done.
class Ceremony {
  constructor(groupPK, message, commitments) {
    const roster = Buffer.concat(commitments.map(u8));
    this._h = fn.thCeremonyNew(u8(groupPK), u8(message), u8(message).length, roster, roster.length);
    if (!this._h) throw new Error('threshold ceremony creation failed');
  }
  sign(card, nonces) { return consume(fn.thSign(this._h, u8(card), u8(card).length, u8(nonces)), 'threshold_sign'); }
  verifyShare(id, publicShare, sigShare) {
    return fn.thVerifyShare(this._h, id, u8(publicShare), u8(sigShare)) === 1;
  }
  aggregate(ids, sigShares) {
    const flat = Buffer.concat(sigShares.map(u8));
    return consume(fn.thAggregate(this._h, Uint16Array.from(ids), flat, ids.length), 'threshold_aggregate');
  }
  free() { fn.thCeremonyFree(this._h); }
}

// CredentialGate (anonymous membership admission, Option A — unlinkable). An
// issuer signs a member's attributes as a BBS credential; a holder admits itself
// to a role by proving in zero knowledge it holds an issuer-signed credential
// whose tier clears the policy and whose expiry has not passed.
class CredentialPolicy {
  // mode: 0 = unlinkable (Option A, default), 1 = pseudonymous (Option B).
  constructor(chainId, issuerPublic, header, attributeCount = 4, minTier = 0, mode = 0) {
    this.encoded = mode === 0
      ? consume(fn.credPolicyEncode(BigInt(chainId), u8(issuerPublic), u8(issuerPublic).length,
          u8(header), u8(header).length, attributeCount, minTier), 'credential_policy_encode')
      : consume(fn.credPolicyEncodeEx(BigInt(chainId), u8(issuerPublic), u8(issuerPublic).length,
          u8(header), u8(header).length, attributeCount, minTier, mode), 'credential_policy_encode_ex');
  }
  // 32-byte commitment to commit into genesis.
  hash() { return consume(fn.credPolicyHash(this.encoded, this.encoded.length), 'credential_policy_hash'); }
  // Issuer side: assemble + BBS-sign a credential (holder-secret blob).
  issue(issuerSecret, memberSecret, tier, notAfter, issuerEpoch) {
    return consume(fn.credIssue(this.encoded, this.encoded.length, u8(issuerSecret), u8(issuerSecret).length,
      u8(memberSecret), u8(memberSecret).length, tier, BigInt(notAfter), issuerEpoch), 'credential_issue');
  }
  // Holder side: an unlinkable presentation for (epoch, scope, challenge).
  present(credential, epoch, scope, challenge) {
    return consume(fn.credPresent(this.encoded, this.encoded.length, u8(credential), u8(credential).length,
      epoch, scope, u8(challenge), u8(challenge).length), 'credential_present');
  }

  // ── Option B — pseudonymous (per-epoch Sybil resistance) ───────────────────
  // Holder step 1: derive a nym seed from memberSecret and blind-commit it. The
  // returned blob is holder-secret; extractCommitment(blob) is sent to the issuer.
  membershipCommit(memberSecret) {
    return consume(fn.membershipCommit(this.encoded, this.encoded.length,
      u8(memberSecret), u8(memberSecret).length), 'membership_commit');
  }
  // Issuer step 2: blind-sign the holder's commitment (issuer never sees the seed).
  blindIssue(issuerSecret, commitment, tier, notAfter, issuerEpoch) {
    return consume(fn.blindIssue(this.encoded, this.encoded.length, u8(issuerSecret), u8(issuerSecret).length,
      u8(commitment), u8(commitment).length, tier, BigInt(notAfter), issuerEpoch), 'blind_issue');
  }
  // Holder step 3: an unlinkable-per-epoch pseudonymous presentation.
  presentPseudonymous(credential, epoch, scope, challenge) {
    return consume(fn.credPresentPseud(this.encoded, this.encoded.length, u8(credential), u8(credential).length,
      epoch, scope, u8(challenge), u8(challenge).length), 'credential_present_pseudonymous');
  }
}
// The commitment is the first length-prefixed field of a membership-commit blob.
function extractCommitment(membershipCommit) {
  const b = u8(membershipCommit);
  const n = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
  return b.subarray(4, 4 + n);
}
// Holder step 3 (assemble): finalize the pseudonymous credential (holder-secret).
function finalizeMembership(membershipCommit, blindIssuance) {
  return consume(fn.finalizeMembership(u8(membershipCommit), u8(membershipCommit).length,
    u8(blindIssuance), u8(blindIssuance).length), 'finalize_membership');
}
class CredentialGate {
  constructor(policy) { this._h = fn.credGateNew(policy.encoded, policy.encoded.length); }
  revokeIssuerEpoch(epoch) { fn.credGateRevoke(this._h, epoch); }
  // Verifier side: admit? Total 1/0 — a malformed presentation is a quiet false.
  admit(epoch, scope, challenge, presentation, now) {
    return fn.credGateAdmit(this._h, epoch, scope, u8(challenge), u8(challenge).length,
      u8(presentation), u8(presentation).length, BigInt(now)) === 1;
  }
  // Option B: admit? Returns the 48-byte pseudonym on success, or null on reject.
  admitPseudonymous(epoch, scope, challenge, presentation, now) {
    const nym = consume(fn.credGateAdmitPseud(this._h, epoch, scope, u8(challenge), u8(challenge).length,
      u8(presentation), u8(presentation).length, BigInt(now)), 'admit_pseudonymous');
    return nym.length === 0 ? null : nym;
  }
  // §6.2 targeted revocation of an exact pseudonym.
  revokeNym(nym) { fn.credGateRevokeNym(this._h, u8(nym), u8(nym).length); }
  // §6.3 Sybil cap: record an admitted nym; true if first this (epoch, scope).
  noteAdmission(epoch, scope, nym) {
    return fn.credGateNoteAdmission(this._h, epoch, scope, u8(nym), u8(nym).length) === 1;
  }
  free() { fn.credGateFree(this._h); }
}

// Composable chains: a chain is a MIXTURE of pluggable components (engine,
// state model, proposer policy) named by stable ids, committed into the genesis
// hash. Unknown ids / posture-forbidden mixes are rejected at assembly.
const decodeIds = (b) => {
  const n = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
  const out = []; let off = 4;
  for (let i = 0; i < n; i++) {
    const l = (b[off] << 24) | (b[off+1] << 16) | (b[off+2] << 8) | b[off+3]; off += 4;
    out.push(b.subarray(off, off + l).toString('utf8')); off += l;
  }
  return out;
};
const u64arr = (xs) => xs.length ? BigUint64Array.from(xs.map((x) => BigInt(x))) : null;
class Blueprint {
  constructor(consensus, stateModel, proposer = '', params = Buffer.alloc(0)) {
    this.consensus = consensus; this.stateModel = stateModel; this.proposer = proposer; this.params = u8(params);
    this.encoded = consume(fn.bpEncode(consensus, stateModel, proposer, this.params, this.params.length), 'blueprint_encode');
  }
  hash() { return consume(fn.bpHash(this.encoded, this.encoded.length), 'blueprint_hash'); }
}
// Assembled node: the engine the mixture named, behind propose → (seal|mine) → submit.
class Chain {
  constructor(h) { this._h = h; }
  get height() { return Number(fn.chainHeight(this._h)); }
  blueprint() { return consume(fn.chainBlueprint(this._h), 'chain_blueprint'); }
  propose(timestamp) { return consume(fn.chainPropose(this._h, BigInt(timestamp)), 'chain_propose'); }
  seal(block) { return consume(fn.chainSeal(this._h, u8(block), u8(block).length), 'chain_seal'); }
  mine(block) { return consume(fn.chainMine(this._h, u8(block), u8(block).length), 'chain_mine'); }
  submit(block, justification = Buffer.alloc(0)) {
    return fn.chainSubmit(this._h, u8(block), u8(block).length, u8(justification), u8(justification).length) === 1;
  }
  free() { fn.chainFree(this._h); }

  // ── Subchains ──
  /** Propose the next block on a PARENT chain, anchoring encoded anchors (ascending child id). Throws on an invalid anchor. */
  proposeAnchored(timestamp, anchors) {
    const blob = blobList(anchors);
    return consume(fn.chainProposeAnchored(this._h, BigInt(timestamp), blob, blob.length), 'chain_propose_anchored');
  }
  /** BFT: this validator's precommit aggregated into a commit certificate (the justification for submit; what a parent anchors). */
  certify(block) { return consume(fn.chainCertify(this._h, u8(block), u8(block).length), 'chain_certify'); }
  /** BFT: schedule a coordinated validator-set change ([{publicKey, power}]); the next proposal announces it. Throws if not BFT / malformed. */
  scheduleValidators(validators) {
    const pks = Buffer.concat(validators.map((v) => u8(v.publicKey)));
    if (fn.chainScheduleValidators(this._h, pks, u64arr(validators.map((v) => v.power)), validators.length) !== 1)
      throw new Error('chain: cannot schedule validators (not BFT, or malformed set)');
  }
  /** BFT: the validator set in force for the next block → [{publicKey, power}]. */
  validators() { return decodeValidatorSet(consume(fn.chainValidators(this._h), 'chain_validators')); }
  /** A parent's last anchored header for a child. */
  childTip(childChainId) { return consume(fn.chainChildTip(this._h, BigInt(childChainId)), 'chain_child_tip'); }
  /** A proof hop from this parent's current registry into a child. */
  subchainHop(childChainId) { return consume(fn.chainSubchainHop(this._h, BigInt(childChainId)), 'chain_subchain_hop'); }
  /** State-inclusion proof for a 32-byte address on this account-model chain. */
  proveAccount(address) { return consume(fn.chainProveAccount(this._h, u8(address)), 'chain_prove_account'); }
}
// The canonical ValidatorSet encoding: [count]([len][pk][power u64])×.
function decodeValidatorSet(b) {
  const n = b.readUInt32BE(0); const out = []; let off = 4;
  for (let i = 0; i < n; i++) { const l = b.readUInt32BE(off); off += 4; const pk = b.subarray(off, off + l); off += l; out.push({ publicKey: pk, power: Number(b.readBigUInt64BE(off)) }); off += 8; }
  return out;
}
function encodeValidatorSet(vs) {
  const parts = [Buffer.alloc(4)]; parts[0].writeUInt32BE(vs.length);
  for (const v of vs) { const l = Buffer.alloc(4); l.writeUInt32BE(u8(v.publicKey).length); const p = Buffer.alloc(8); p.writeBigUInt64BE(BigInt(v.power)); parts.push(l, u8(v.publicKey), p); }
  return Buffer.concat(parts);
}
/** Skip results of LightClient.acceptSkipping. */
const SkipRefused = 0, SkipAccepted = 1, SkipInsufficientOverlap = 2;
/** A light client: follows a header chain with certificates only. Sequential accept() plus the
 *  OVERLAP (skipping) rule — acceptSkipping({header, certificate, validators}) jumps to a far header
 *  when its own set certifies it and the signers hold more than the trust level (default 1/3) of the
 *  set the client already trusts; syncTo bisects when the overlap is too thin. */
class LightClient {
  constructor(chainId, validators, trustedHeader) {
    const vs = encodeValidatorSet(validators);
    this._h = fn.lcNew(BigInt(chainId), vs, vs.length, u8(trustedHeader), u8(trustedHeader).length);
    if (!this._h) throw new Error('light client: malformed validator set or header');
  }
  get height() { return Number(fn.lcHeight(this._h)); }
  tip() { return consume(fn.lcTip(this._h), 'light_client_tip'); }
  validators() { return decodeValidatorSet(consume(fn.lcValidators(this._h), 'light_client_validators')); }
  setTrustLevel(numerator, denominator) { fn.lcSetTrust(this._h, numerator, denominator); }
  setMaxSkip(blocks) { fn.lcSetMaxSkip(this._h, BigInt(blocks)); }
  accept(header, certificate) { return fn.lcAccept(this._h, u8(header), u8(header).length, u8(certificate), u8(certificate).length) === 1; }
  acceptSkipping({ header, certificate, validators }) {
    const vs = encodeValidatorSet(validators);
    return fn.lcAcceptSkipping(this._h, u8(header), u8(header).length, u8(certificate), u8(certificate).length, vs, vs.length);
  }
  /** Bisection: fetch(height) → checkpoint | null. Throws on a hard refusal / missing checkpoint. */
  syncTo(target, targetHeight, fetch) {
    const stack = [[target, targetHeight]];
    for (let n = 0; stack.length; n++) {
      if (n > 128) throw new Error('light client: bisection exceeded its fetch budget');
      const [cp, h] = stack[stack.length - 1];
      const r = this.acceptSkipping(cp);
      if (r === SkipAccepted) { stack.pop(); continue; }
      if (r !== SkipInsufficientOverlap) throw new Error('light client: checkpoint refused');
      const lo = this.height, hi = h;
      if (hi - lo <= 1) throw new Error('light client: insufficient overlap and no intermediate header');
      const mid = lo + Math.floor((hi - lo) / 2);
      const m = fetch(mid);
      if (!m) throw new Error('light client: checkpoint unavailable at ' + mid);
      stack.push([m, mid]);
    }
  }
  free() { if (this._h) { fn.lcFree(this._h); this._h = null; } }
}

// ── Chain tree: declare a FAMILY of chains (root, subchains, custom mixtures) and drive it ──
// node = { chainId, blueprint?: Blueprint (default bft/account), assuranceLevel?: 1, accounts?: [{address, balance}],
//          validators: [{publicKey, secret?, power}], maxAnchorsPerBlock?, maxHeadersPerAnchor?, children?: [node] }
function encodeTreeNode(node) {
  const bpEnc = (node.blueprint || new Blueprint('bft', 'account')).encoded;
  const accounts = node.accounts || [], validators = node.validators || [], children = node.children || [];
  const addrs = Buffer.concat(accounts.map((a) => u8(a.address)));
  const pks = Buffer.concat(validators.map((v) => u8(v.publicKey)));
  const secrets = Buffer.concat(validators.map((v) => (v.secret ? u8(v.secret) : Buffer.alloc(32))));
  const kids = blobList(children.map(encodeTreeNode));
  return consume(fn.treeNodeEncode(BigInt(node.chainId), bpEnc, bpEnc.length, node.assuranceLevel || 1,
    addrs, u64arr(accounts.map((a) => a.balance)), accounts.length,
    pks, validators.length ? secrets : null, u64arr(validators.map((v) => v.power)), validators.length,
    node.maxAnchorsPerBlock || 0, node.maxHeadersPerAnchor || 0, kids, kids.length), 'chain_tree_node_encode');
}
/** Why a spec is structurally invalid ('' = valid). */
function chainTreeSpecError(spec) { return consume(fn.treeSpecError(u8(spec), u8(spec).length), 'chain_tree_spec_error').toString(); }
const treeKeep = [];
/** A built family of chains driven in-process: produce() anchors children upward automatically;
 *  proveAccount() proves any descendant from the root header. Hooks: onFinalized(fn), txSource(fn). */
class ChainTree {
  constructor(rootNode) {
    const spec = encodeTreeNode(rootNode);
    const err = chainTreeSpecError(spec);
    if (err) throw new Error('chain tree: ' + err);
    this._h = fn.treeBuild(spec, spec.length);
    if (!this._h) throw new Error('chain tree: build rejected (unknown mixture or posture-forbidden engine)');
  }
  chainIds() { const b = consume(fn.treeChainIds(this._h), 'chain_tree_chain_ids'); const n = b.readUInt32BE(0); const out = []; for (let i = 0; i < n; i++) out.push(Number(b.readBigUInt64BE(4 + 8 * i))); return out; }
  height(chainId) { return Number(fn.treeHeight(this._h, BigInt(chainId))); }
  genesis(chainId) { return consume(fn.treeGenesis(this._h, BigInt(chainId)), 'chain_tree_genesis'); }
  tip(chainId) { return consume(fn.treeTip(this._h, BigInt(chainId)), 'chain_tree_tip'); }
  pendingAnchors(chainId) { return fn.treePending(this._h, BigInt(chainId)); }
  produce(chainId, timestamp) { return consume(fn.treeProduce(this._h, BigInt(chainId), BigInt(timestamp)), 'chain_tree_produce'); }
  proveAccount(chainId, address) { return consume(fn.treeProve(this._h, BigInt(chainId), u8(address)), 'chain_tree_prove_account'); }
  verify(proof) { return fn.treeVerify(this._h, u8(proof), u8(proof).length) === 1; }
  /** fn(chainId, block: Buffer) for every finalized block. */
  onFinalized(cb) {
    const reg = koffi.register((_ctx, chainId, block, len) => { try { cb(Number(chainId), Buffer.from(koffi.decode(block, 'uint8_t', Number(len)))); } catch (_) {} }, koffi.pointer('AviaryFinalizedFn'));
    treeKeep.push(reg); fn.treeSetFinalized(this._h, reg, null);
  }
  /** fn(chainId, timestamp) → [encodedTx] to include when that chain produces. */
  txSource(cb) {
    const reg = koffi.register((_ctx, chainId, ts, out, cap, outLen) => {
      try { const blob = blobList(cb(Number(chainId), Number(ts)) || []); if (blob.length > cap) return 0; koffi.encode(out, 'uint8_t', blob, blob.length); koffi.encode(outLen, 'size_t', blob.length); return 1; } catch (_) { return 0; }
    }, koffi.pointer('AviaryTxSourceFn'));
    treeKeep.push(reg); fn.treeSetTxSource(this._h, reg, null);
  }
  free() { if (this._h) { fn.treeFree(this._h); this._h = null; } }
}

// [count u32]([len u32][bytes]) repeated, decoded back into Buffers.
function splitBlobList(b) {
  const n = b.readUInt32BE(0); const out = []; let off = 4;
  for (let i = 0; i < n; i++) { const l = b.readUInt32BE(off); off += 4; out.push(Buffer.from(b.subarray(off, off + l))); off += l; }
  return out;
}
// [count u32]([len u32][bytes])× — the C ABI's blob-list convention.
function blobList(items) {
  const parts = [Buffer.alloc(4)];
  parts[0].writeUInt32BE(items.length);
  for (const it of items) { const l = Buffer.alloc(4); l.writeUInt32BE(u8(it).length); parts.push(l, u8(it)); }
  return Buffer.concat(parts);
}
// ── Subchains: a parent block anchors a child chain's finalized headers ───────
// (docs/design/SUBCHAINS.md). Register a child at the parent's genesis by its
// genesis HEADER + fixed validator set; anchor its certified headers; prove a
// state entry on a descendant from the parent's header. v1 anchors BFT children.
const genesisHeader = (genesis) => consume(fn.genesisHeader(u8(genesis), u8(genesis).length), 'genesis_header');
function encodeChildRegistration({ chainId, genesisHeader: hdr, validators, maxDepth = 0 }) {
  const pks = Buffer.concat(validators.map((v) => u8(v.publicKey)));
  return consume(fn.childRegEncode(BigInt(chainId), u8(hdr), u8(hdr).length, pks, u64arr(validators.map((v) => v.power)), validators.length, maxDepth), 'child_registration_encode');
}
function encodeSubchainPolicy({ maxChildren, maxDepth = 1, maxAnchorsPerBlock = 0, maxHeadersPerAnchor = 0, children = [] }) {
  const blob = blobList(children.map((c) => Buffer.isBuffer(c) ? c : encodeChildRegistration(c)));
  return consume(fn.subchainPolicyEncode(maxChildren, maxDepth, maxAnchorsPerBlock, maxHeadersPerAnchor, blob, blob.length), 'subchain_policy_encode');
}
const commitSubchainPolicy = (genesis, policy) => consume(fn.genesisCommitSubchains(u8(genesis), u8(genesis).length, u8(policy), u8(policy).length), 'genesis_commit_subchain_policy');
const commitParentBinding = (genesis, parentChainId, parentGenesisHash) => consume(fn.genesisCommitBinding(u8(genesis), u8(genesis).length, BigInt(parentChainId), u8(parentGenesisHash)), 'genesis_commit_parent_binding');
const blockHeader = (block) => consume(fn.blockHeader(u8(block), u8(block).length), 'block_header');
const headerSubchainRoot = (header) => consume(fn.headerSubchainRoot(u8(header), u8(header).length), 'header_subchain_root');
const certifiedHeader = (header, certificate) => consume(fn.certifiedHeaderEncode(u8(header), u8(header).length, u8(certificate), u8(certificate).length), 'certified_header_encode');
function encodeAnchor(childChainId, certifiedHeaders) {
  const blob = blobList(certifiedHeaders);
  return consume(fn.anchorEncode(BigInt(childChainId), blob, blob.length), 'anchor_encode');
}
function encodeSubchainProof(hops, stateProof) {
  const blob = blobList(hops);
  return consume(fn.subchainProofEncode(blob, blob.length, u8(stateProof), u8(stateProof).length), 'subchain_proof_encode');
}
const verifySubchainProof = (parentSubchainRoot, proof) => u8(parentSubchainRoot).length === 32 && fn.subchainProofVerify(u8(parentSubchainRoot), u8(proof), u8(proof).length) === 1;

function buildGenesis({ chainId, timestamp = 1, assuranceLevel = 1, stateModel = 0, accounts = [], validators = [] }) {
  const addrs = Buffer.concat(accounts.map((a) => u8(a.address)));
  const pks = Buffer.concat(validators.map((v) => u8(v.publicKey)));
  return consume(fn.genesisBuild(BigInt(chainId), BigInt(timestamp), assuranceLevel, stateModel,
    addrs, u64arr(accounts.map((a) => a.balance)), accounts.length,
    pks, u64arr(validators.map((v) => v.power)), validators.length), 'genesis_build');
}
// ── Foreign components: engines / proposers implemented in JavaScript ─────────
// The consensus pipeline stays native (propose/apply go through the Ledger); JS
// supplies the JUSTIFICATION policy (justify a proposed block / verify one) or
// the proposer (leader-election) policy. Callbacks run synchronously inside
// consensus: they MUST be deterministic (no clock, no randomness, no Math.random)
// and must not call back into the chain. Registered callbacks live for the
// process lifetime (chain identity — never rename an id).
const MAX_JUSTIFICATION = 4096;
const foreignKeep = [];
// engine: { justify(block: Buffer): Buffer | null, verify(block: Buffer, justification: Buffer): boolean }
function registerEngine(id, engine, grindProof = false) {
  if (typeof engine.justify !== 'function' || typeof engine.verify !== 'function') throw new TypeError('engine needs justify() and verify()');
  const justify = koffi.register((_ctx, block, len, out, cap, outLen) => {
    try {
      const j = engine.justify(Buffer.from(koffi.decode(block, 'uint8_t', len)));
      if (!j || j.length > cap) return 0;
      koffi.encode(out, 'uint8_t', j, j.length);
      koffi.encode(outLen, 'size_t', j.length);
      return 1;
    } catch (e) { if (process.env.AVIARY_FOREIGN_DEBUG) console.error('justify callback threw:', e); return 0; }
  }, koffi.pointer('AviaryJustifyFn'));
  const verify = koffi.register((_ctx, block, len, just, jlen) => {
    try {
      return engine.verify(Buffer.from(koffi.decode(block, 'uint8_t', len)), Buffer.from(koffi.decode(just, 'uint8_t', jlen))) ? 1 : 0;
    } catch (e) { if (process.env.AVIARY_FOREIGN_DEBUG) console.error('verify callback threw:', e); return 0; }
  }, koffi.pointer('AviaryVerifyFn'));
  foreignKeep.push(justify, verify);
  return fn.regAddEngine(id, justify, verify, null, grindProof ? 1 : 0) === 1;
}
// policy(height: bigint, validators: number) => number  (proposer index)
function registerProposer(id, policy) {
  const cb = koffi.register((_ctx, height, n) => {
    try { return policy(height, n) >>> 0; } catch (_) { return 0; }
  }, koffi.pointer('AviaryProposerFn'));
  foreignKeep.push(cb);
  return fn.regAddProposer(id, cb, null) === 1;
}

// null = rejected (fail-closed): unknown id, state mismatch, or posture-forbidden engine.
function assembleChain(genesis, me, blsPublic, blsSecret) {
  const h = fn.chainAssemble(u8(genesis), u8(genesis).length, me, u8(blsPublic), u8(blsPublic).length, u8(blsSecret), u8(blsSecret).length);
  return h ? new Chain(h) : null;
}

// Parse a share card's identity blob [id u16 BE][group_pk 32][public 32].
function shareInfo(card) {
  const b = consume(fn.thShareInfo(u8(card), u8(card).length), 'threshold_share_info');
  return { index: (b[0] << 8) | b[1], groupPublicKey: b.subarray(2, 34), publicShare: b.subarray(34, 66) };
}

// -- Networking: consensus over a network the app owns --
// A NetNode is a validator (or observer) running BFT consensus over a gossip
// mesh with no socket and no clock. Every input returns { relays, timeouts }:
// send each relay to that peer over your transport, arm each timeout for a
// duration you choose, and feed what arrives back through onGossip/onTimeout.
const NET_OBSERVER = 0xFFFFFFFF;
const StepPropose = 0, StepPrevote = 1, StepPrecommit = 2;
function decodeOutbound(b) {
  let off = 0; const relays = []; const timeouts = [];
  const rc = b.readUInt32BE(off); off += 4;
  for (let i = 0; i < rc; i++) { const to = b.readUInt32BE(off); const l = b.readUInt32BE(off + 4); off += 8; relays.push({ to, message: Buffer.from(b.subarray(off, off + l)) }); off += l; }
  const tc = b.readUInt32BE(off); off += 4;
  for (let i = 0; i < tc; i++) { timeouts.push({ step: b[off], round: b.readUInt32BE(off + 1) }); off += 5; }
  return { relays, timeouts };
}
class NetNode {
  /** { genesis, me, blsKey?: {public, secret}, timestamp?: 1, peers?: [] }; me = NET_OBSERVER follows without voting. */
  constructor({ genesis, me, blsKey = null, timestamp = 1, peers = [] }) {
    const g = u8(genesis), pk = blsKey ? u8(blsKey.public) : Buffer.alloc(0), sk = blsKey ? u8(blsKey.secret) : Buffer.alloc(0);
    this._h = fn.netNew(g, g.length, me, pk, pk.length, sk, sk.length, BigInt(timestamp));
    if (!this._h) throw new Error('net node: malformed genesis or key');
    for (const p of peers) this.addPeer(p);
  }
  addPeer(peer) { fn.netAddPeer(this._h, peer); }
  /** Encoded transactions this node proposes at the current height; cleared by advance(). */
  setTransactions(encodedTxs) { const blob = blobList(encodedTxs); if (fn.netSetTxs(this._h, blob, blob.length) !== 1) throw new Error('net node: malformed transaction'); }
  start() { return decodeOutbound(consume(fn.netStart(this._h), 'net_node_start')); }
  onGossip(from, payload) { return decodeOutbound(consume(fn.netOnGossip(this._h, from, u8(payload), u8(payload).length), 'net_node_on_gossip')); }
  onTimeout({ step, round }) { return decodeOutbound(consume(fn.netOnTimeout(this._h, step, round), 'net_node_on_timeout')); }
  advance() { return decodeOutbound(consume(fn.netAdvance(this._h), 'net_node_advance')); }
  get committed() { return fn.netCommitted(this._h) === 1; }
  get height() { return Number(fn.netHeight(this._h)); }
  peerBanned(peer) { return fn.netPeerBanned(this._h, peer) === 1; }
  replayLog() { return splitBlobList(consume(fn.netReplayLog(this._h), 'net_node_replay_log')); }
  tip() { return consume(fn.netTip(this._h), 'net_node_tip'); }
  stateRoot() { return consume(fn.netStateRoot(this._h), 'net_node_state_root'); }
  balance(address) { return Number(fn.netBalance(this._h, u8(address))); }
  proveAccount(address) { return consume(fn.netProve(this._h, u8(address)), 'net_node_prove_account'); }
  free() { if (this._h) { fn.netFree(this._h); this._h = null; } }
}
/** A fresh long term identity: { kem: {public, secret}, sig: {public, secret} }. */
function generateSecureIdentity() {
  const p = splitBlobList(consume(fn.secIdentity(), 'secure_identity_generate'));
  return { kem: { public: p[0], secret: p[1] }, sig: { public: p[2], secret: p[3] } };
}
/** A confidential, mutually authenticated, post quantum hybrid link. Pin peerIdentity against the key you expect. */
class SecureSession {
  static _wrap(h) { const s = Object.create(SecureSession.prototype); s._h = h; return s; }
  /** Dialing side: encapsulate to the responder's KEM public key, authenticate with sig {public, secret}. */
  static initiate(responderKemPublic, sig) {
    const rk = u8(responderKemPublic), sp = u8(sig.public), ss = u8(sig.secret);
    const h = fn.secInitiate(rk, rk.length, sp, sp.length, ss, ss.length);
    if (!h) throw new Error('secure session: initiate failed');
    return SecureSession._wrap(h);
  }
  /** Accepting side: consume the initiator's first message under identity; open at once. */
  static accept(identity, firstMessage) {
    const kp = u8(identity.kem.public), ks = u8(identity.kem.secret), sp = u8(identity.sig.public), ss = u8(identity.sig.secret), m = u8(firstMessage);
    const h = fn.secAccept(kp, kp.length, ks, ks.length, sp, sp.length, ss, ss.length, m, m.length);
    if (!h) throw new Error('secure session: initiator refused');
    return SecureSession._wrap(h);
  }
  /** The handshake message this side must send: msg1 for an initiator, the reply for a responder. */
  get handshakeMessage() { return consume(fn.secHandshake(this._h), 'secure_handshake_message'); }
  /** Initiator only: consume the responder's reply. Throws when the responder does not verify. */
  complete(reply) { if (fn.secComplete(this._h, u8(reply), u8(reply).length) !== 1) throw new Error('secure session: responder refused'); }
  get open() { return fn.secOpen(this._h) === 1; }
  get peerIdentity() { return consume(fn.secPeer(this._h), 'secure_peer_identity'); }
  encrypt(plaintext) { return consume(fn.secEncrypt(this._h, u8(plaintext), u8(plaintext).length), 'secure_encrypt'); }
  decrypt(frame) { return consume(fn.secDecrypt(this._h, u8(frame), u8(frame).length), 'secure_decrypt'); }
  free() { if (this._h) { fn.secFree(this._h); this._h = null; } }
}
/** Rebuild a node from Node#snapshot(); every block is replayed through consensus. Throws on a malformed blob. */
function restoreNode(snapshot) {
  const b = u8(snapshot); const h = fn.nodeRestore(b, b.length);
  if (!h) throw new Error('node restore refused (malformed snapshot)');
  return Node._wrap(h);
}

module.exports = {
  preload,
  _warm,
  init() { if (fn.init() !== 0) throw new Error('aviary init failed'); },
  version: () => fn.version(),
  cryptoBackendVersion: () => fn.backend(),

  hash: (msg) => consume(fn.hash(u8(msg), u8(msg).length), 'hash'),
  verifyStateProof: (root, proof) => fn.verifyState(u8(root), u8(proof), u8(proof).length) === 1,
  verifyTxInclusion: (root, proof) => fn.verifyTx(u8(root), u8(proof), u8(proof).length) === 1,

  newNode: (chainId) => new Node(chainId),
  Node,
  accountAddress: Node.accountAddress,

  newKeyStore: (passphrase) => new KeyStore(passphrase),
  loadKeyStore: (blob) => KeyStore.load(blob),
  KeyStore,
  restoreNode,

  // Networking
  NetNode,
  newNetNode: (opts) => new NetNode(opts),
  NET_OBSERVER,
  StepPropose, StepPrevote, StepPrecommit,
  SecureSession,
  generateSecureIdentity,
  initiateSecureSession: SecureSession.initiate,
  acceptSecureSession: SecureSession.accept,

  // Threshold (FROST t-of-n) accounts. Cards, commit, ceremony, aggregate.
  createThresholdAccount(n, t) {
    const blob = consume(fn.thCreate(n, t), 'threshold_create');
    const count = (blob[0] << 8) | blob[1];
    const cards = [];
    let off = 2;
    for (let i = 0; i < count; i++) {
      const len = (blob[off] << 24) | (blob[off + 1] << 16) | (blob[off + 2] << 8) | blob[off + 3];
      off += 4;
      cards.push(Buffer.from(blob.subarray(off, off + len)));
      off += len;
    }
    return cards;
  },
  shareInfo,

  // AssuranceProfile: what tier a posture (1=standard, 2=hardened, 3=fortress)
  // mandates at every seam.
  describeAssurance(level) {
    const b = consume(fn.asDescribe(level), 'assurance_describe');
    return {
      ratcheted: b[0] === 1, vrf: b[1] === 1, thresholdCustody: b[2] === 1, sealedMempool: b[3] === 1,
      sigAlgos: Array.from(b.subarray(5, 5 + b[4])),
    };
  },

  // Encode a transaction from its fields (e.g. to seal into a SealedMempool).
  encodeTx({ version = 2, chainId = 0, sigAlgo = 1, fee = 0, body, author, signature }) {
    return consume(fn.txEncode(version, BigInt(chainId), sigAlgo, BigInt(fee),
      u8(body), u8(body).length, u8(author), u8(author).length, u8(signature), u8(signature).length), 'tx_encode');
  },
  // Seal an encoded tx to the proposer's hybrid-KEM public key.
  sealTx: (txEncoded, proposerKemPublic) =>
    consume(fn.smSeal(u8(txEncoded), u8(txEncoded).length, u8(proposerKemPublic), u8(proposerKemPublic).length), 'sealed_tx_seal'),
  SealedMempool,

  // CredentialGate (anonymous membership admission, Option A). A BBS issuer key,
  // a canonical policy, and the holder/verifier present/admit flow.
  newBBSIssuer: (keyMaterial) => splitBlob(consume(fn.bbsKeygen(u8(keyMaterial), u8(keyMaterial).length), 'bbs_keygen')),
  CredentialPolicy,
  CredentialGate,
  extractCommitment,
  finalizeMembership,
  CredentialModePseudonymous: 1,

  // Composable chains.
  newBLSValidator: (keyMaterial) => splitBlob(consume(fn.blsKeygen(u8(keyMaterial), u8(keyMaterial).length), 'bls_keygen')),
  Blueprint,
  engineIds: () => decodeIds(consume(fn.registryIds(0), 'registry_ids')),
  stateModelIds: () => decodeIds(consume(fn.registryIds(1), 'registry_ids')),
  proposerIds: () => decodeIds(consume(fn.registryIds(2), 'registry_ids')),
  buildGenesis,
  commitBlueprint: (genesis, bp) => consume(fn.genesisCommitBp(u8(genesis), u8(genesis).length, bp.encoded, bp.encoded.length), 'genesis_commit_blueprint'),
  genesisHash: (genesis) => consume(fn.genesisHash(u8(genesis), u8(genesis).length), 'genesis_hash'),
  assembleChain,
  Chain,
  registerEngine,
  registerProposer,
  MAX_JUSTIFICATION,
  // Light client.
  LightClient, SkipRefused, SkipAccepted, SkipInsufficientOverlap,
  // Chain tree.
  ChainTree, encodeTreeNode, chainTreeSpecError,
  // Subchains.
  genesisHeader, encodeChildRegistration, encodeSubchainPolicy, commitSubchainPolicy, commitParentBinding,
  blockHeader, headerSubchainRoot, certifiedHeader, encodeAnchor, encodeSubchainProof, verifySubchainProof,

  commit(card) {
    const b = consume(fn.thCommit(u8(card), u8(card).length), 'threshold_commit');
    return { nonces: b.subarray(0, 64), commitment: b.subarray(64, 130) };
  },
  sealShare: (card, passphrase, ops = 0, mem = 0) =>
    consume(fn.thShareSeal(u8(card), u8(card).length, passphrase, BigInt(ops), mem), 'threshold_share_seal'),
  unsealShare: (envelope, passphrase) =>
    consume(fn.thShareUnseal(u8(envelope), u8(envelope).length, passphrase), 'threshold_share_unseal'),
  Ceremony,

  // Portable account export (tiered encrypted backup). Returns/takes accounts as
  // { public, secret } Buffers; envelopes are Buffers.
  hybridKemKeypair: () => splitBlob(consume(fn.kemKeygen(), 'hybrid_kem_keygen')),
  sealAccountPassphrase: (publicKey, secret, passphrase, ops = 0, mem = 0) =>
    consume(fn.pSealPass(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      passphrase, BigInt(ops), mem), 'portable_seal_passphrase'),
  openAccountPassphrase: (envelope, passphrase) =>
    splitBlob(consume(fn.pOpenPass(u8(envelope), u8(envelope).length, passphrase), 'portable_open_passphrase')),
  sealAccountTo: (publicKey, secret, recipientKemPublic) =>
    consume(fn.pSealTo(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      u8(recipientKemPublic), u8(recipientKemPublic).length), 'portable_seal_to'),
  openAccountTo: (envelope, recipientKemSecret) =>
    splitBlob(consume(fn.pOpenTo(u8(envelope), u8(envelope).length,
      u8(recipientKemSecret), u8(recipientKemSecret).length), 'portable_open_to')),
  sealAccountSignedTo: (publicKey, secret, recipientKemPublic, senderSigSecret) =>
    consume(fn.pSealSigned(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      u8(recipientKemPublic), u8(recipientKemPublic).length,
      u8(senderSigSecret), u8(senderSigSecret).length), 'portable_seal_signed_to'),
  openAccountSignedFrom: (envelope, recipientKemSecret, senderSigPublic) =>
    splitBlob(consume(fn.pOpenSigned(u8(envelope), u8(envelope).length,
      u8(recipientKemSecret), u8(recipientKemSecret).length,
      u8(senderSigPublic), u8(senderSigPublic).length), 'portable_open_signed_from')),

  ethAddress: (secret32) => consume(fn.ethAddr(u8(secret32)), 'eth_address').toString('utf8'),
  btcAddress: (secret32) => consume(fn.btcAddr(u8(secret32)), 'btc_address').toString('utf8'),
  solAddress: (ed64) => consume(fn.solAddr(u8(ed64)), 'sol_address').toString('utf8'),
  erc20TransferCalldata: (to20, amount) => consume(fn.erc20(u8(to20), BigInt(amount)), 'erc20'),
  functionSelector: (signature) => consume(fn.selector(signature), 'selector'),
  signLegacyTx({ nonce = 0, gasPrice = 0, gasLimit = 0, to = null, value = 0, data = null, chainId = 1, secret }) {
    const t = to ? u8(to) : null;
    const d = data ? u8(data) : null;
    return consume(fn.signLegacy(BigInt(nonce), BigInt(gasPrice), BigInt(gasLimit),
      t, t ? 1 : 0, BigInt(value), d, d ? d.length : 0, BigInt(chainId), u8(secret)), 'sign_legacy');
  },
};
