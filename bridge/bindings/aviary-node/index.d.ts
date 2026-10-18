// Type definitions for the Aviary Node.js binding (aviary-ledger).

/** Byte input accepted everywhere a binary argument is taken. */
export type Bytes = Buffer | Uint8Array;
/** Integer input accepted for heights, amounts, nonces and timestamps. */
export type Int = number | bigint;

export interface Account {
  /** 64-byte Ed25519 secret (public key = secret[32:64]). */
  secret: Buffer;
  /** 32-byte Ed25519 public key. */
  publicKey: Buffer;
  /** 32-byte account address = BLAKE3-256(publicKey). */
  address: Buffer;
}

export interface SignLegacyTxParams {
  nonce?: number | bigint;
  gasPrice?: number | bigint;
  gasLimit?: number | bigint;
  /** 20-byte recipient; omit/null for contract creation. */
  to?: Buffer | Uint8Array | null;
  value?: number | bigint;
  /** Calldata; omit/null for a plain value transfer. */
  data?: Buffer | Uint8Array | null;
  /** EIP-155 chain id (1 = mainnet). */
  chainId?: number | bigint;
  /** 32-byte secp256k1 secret key. */
  secret: Buffer | Uint8Array;
}

/** A live in-memory chain node (ledger + BFT + mempool + observability). */
export class Node {
  constructor(chainId: number | bigint);
  free(): void;
  newAccount(): Account;
  /** A hybrid Ed25519 + ML-DSA-65 account. */
  newAccountHybrid(): Account;
  static accountAddress(publicKey: Buffer | Uint8Array): Buffer;
  credit(address: Buffer | Uint8Array, amount: number | bigint): void;
  submitTransfer(secret: Buffer | Uint8Array, toAddress: Buffer | Uint8Array, amount: number | bigint, nonce: number | bigint): Buffer;
  /** An Ed25519 transfer carrying a priority fee tip. */
  submitTransferFee(secret: Bytes, toAddress: Bytes, amount: Int, nonce: Int, fee: Int): Buffer;
  /** A transfer signed by a hybrid post quantum account. */
  submitTransferHybrid(secret: Bytes, publicKey: Bytes, toAddress: Bytes, amount: Int, nonce: Int): Buffer;
  produceBlock(timestamp: number | bigint): number;
  height(): number;
  mempoolSize(): number;
  balance(address: Buffer | Uint8Array): number;
  nonce(address: Buffer | Uint8Array): number;
  /** The committed nonce advanced past every pending transfer from the address. */
  nextNonce(address: Buffer | Uint8Array): number;
  stateRoot(): Buffer;
  drainEvents(): string;
  metrics(): string;
  proveAccount(address: Buffer | Uint8Array): Buffer;
  /** The whole chain as one blob; pending transactions excluded. */
  snapshot(): Buffer;
  blockAt(height: number | bigint): Buffer;
  submitTx(encoded: Buffer | Uint8Array): Buffer;
  submitTransferFromStore(store: KeyStore, from: Buffer | Uint8Array, to: Buffer | Uint8Array, amount: number | bigint, nonce: number | bigint, fee?: number | bigint): Buffer;
  /** cb(json) per chain event, synchronously; null removes the hook. */
  onEvent(cb: ((json: string) => void) | null): void;
}

export class KeyStore {
  constructor(passphrase: string);
  static load(blob: Buffer | Uint8Array): KeyStore;
  free(): void;
  addAccount(publicKey: Buffer | Uint8Array, secret: Buffer | Uint8Array): Buffer;
  reveal(address: Buffer | Uint8Array): Buffer;
  serialise(): Buffer;
  unlock(passphrase: string): void;
  readonly unlocked: boolean;
  signTransfer(args: { chainId: number | bigint; from: Buffer | Uint8Array; to: Buffer | Uint8Array; amount: number | bigint; nonce: number | bigint; fee?: number | bigint }): Buffer;
  /** A record write signed by the stored account at from; the scheme follows the stored key. */
  signRecord(args: { chainId: Int; from: Bytes; nonce: Int; key: Bytes; op: RecordOpValue; value?: Bytes }): Buffer;
  publicKey(address: Buffer | Uint8Array): Buffer;
  has(address: Buffer | Uint8Array): boolean;
  addresses(): Buffer[];
  lock(): void;
}

export interface Relay { to: number; message: Buffer }
export interface TimeoutRequest { step: number; round: number }
export interface Outbound { relays: Relay[]; timeouts: TimeoutRequest[] }
export interface KeyPair { public: Buffer; secret: Buffer }
export interface SecureIdentity { kem: KeyPair; sig: KeyPair }
export const NET_OBSERVER: number;
export const StepPropose: number;
export const StepPrevote: number;
export const StepPrecommit: number;

/** A validator or observer running BFT over a gossip mesh with no socket and no clock of its own. */
export class NetNode {
  constructor(opts: { genesis: Buffer | Uint8Array; me: number; blsKey?: KeyPair | null; timestamp?: number | bigint; peers?: number[] });
  addPeer(peer: number): void;
  setTransactions(encodedTxs: Array<Buffer | Uint8Array>): void;
  start(): Outbound;
  onGossip(from: number, payload: Buffer | Uint8Array): Outbound;
  onTimeout(t: TimeoutRequest): Outbound;
  advance(): Outbound;
  readonly committed: boolean;
  readonly height: number;
  peerBanned(peer: number): boolean;
  replayLog(): Buffer[];
  tip(): Buffer;
  stateRoot(): Buffer;
  balance(address: Buffer | Uint8Array): number;
  proveAccount(address: Buffer | Uint8Array): Buffer;
  /** Record model networks: the committed record at key, or null. */
  record(key: Bytes): RecordEntry | null;
  recordNonce(address: Bytes): number;
  proveRecord(key: Bytes): Buffer;
  /** Buffered events (a block_finalized per committed height) as a JSON array; empty while onEvent is set. */
  drainEvents(): string;
  /** cb(json) for every event, synchronously from the input that caused it; null removes the hook. */
  onEvent(cb: ((json: string) => void) | null): void;
  /** Prometheus text: height, blocks finalized, gossip received, relayed and dropped, timeouts, bans. */
  metrics(): string;
  free(): void;
}
export function newNetNode(opts: ConstructorParameters<typeof NetNode>[0]): NetNode;

/** A confidential, mutually authenticated, post quantum hybrid link between two peers. */
export class SecureSession {
  static initiate(responderKemPublic: Buffer | Uint8Array, sig: KeyPair): SecureSession;
  static accept(identity: SecureIdentity, firstMessage: Buffer | Uint8Array): SecureSession;
  readonly handshakeMessage: Buffer;
  complete(reply: Buffer | Uint8Array): void;
  readonly open: boolean;
  readonly peerIdentity: Buffer;
  encrypt(plaintext: Buffer | Uint8Array): Buffer;
  decrypt(frame: Buffer | Uint8Array): Buffer;
  free(): void;
}
export function generateSecureIdentity(): SecureIdentity;
export function initiateSecureSession(responderKemPublic: Buffer | Uint8Array, sig: KeyPair): SecureSession;
export function acceptSecureSession(identity: SecureIdentity, firstMessage: Buffer | Uint8Array): SecureSession;
export function restoreNode(snapshot: Buffer | Uint8Array, opts?: { replay?: boolean }): Node;
export function newKeyStore(passphrase: string): KeyStore;
export function loadKeyStore(blob: Buffer | Uint8Array): KeyStore;

/** Optionally warm the native library off the main thread (worker_thread). */
export function preload(): Promise<boolean>;
/** Initialize the crypto backend (idempotent; the lazy path calls it for you). */
export function init(): void;
export function version(): string;
export function cryptoBackendVersion(): string;

export function hash(msg: Buffer | Uint8Array): Buffer;
export function verifyStateProof(stateRoot: Buffer | Uint8Array, proof: Buffer | Uint8Array): boolean;
export function verifyTxInclusion(txRoot: Buffer | Uint8Array, proof: Buffer | Uint8Array): boolean;

export function newNode(chainId: number | bigint): Node;
export function accountAddress(publicKey: Buffer | Uint8Array): Buffer;

export function ethAddress(secret32: Buffer | Uint8Array): string;
export function btcAddress(secret32: Buffer | Uint8Array): string;
export function solAddress(edSecret64: Buffer | Uint8Array): string;
export function erc20TransferCalldata(to20: Buffer | Uint8Array, amount: number | bigint): Buffer;
export function functionSelector(signature: string): Buffer;
export function signLegacyTx(params: SignLegacyTxParams): Buffer;

// -- Composable chains ---------------------------------------------------------

export interface ValidatorEntry { publicKey: Bytes; power: Int }
export interface ValidatorInfo { publicKey: Buffer; power: number }
export interface GenesisAccount { address: Bytes; balance: Int }

/** A declared mixture: consensus engine, state model, proposer policy and params. */
export class Blueprint {
  constructor(consensus: string, stateModel: string, proposer?: string, params?: Bytes);
  readonly consensus: string;
  readonly stateModel: string;
  readonly proposer: string;
  readonly params: Buffer;
  /** The canonical encoding committed into a genesis. */
  readonly encoded: Buffer;
  hash(): Buffer;
}

/**
 * Builds a canonical genesis. stateModel 0 = account, 1 = UTXO, 2 = record;
 * assuranceLevel 1 = standard, 2 = hardened, 3 = fortress.
 */
export function buildGenesis(opts: {
  chainId: Int;
  timestamp?: Int;
  assuranceLevel?: number;
  stateModel?: number;
  accounts?: GenesisAccount[];
  validators?: ValidatorEntry[];
}): Buffer;
/** Commits a mixture into a genesis; the genesis hash changes because the mixture is chain identity. */
export function commitBlueprint(genesis: Bytes, blueprint: Blueprint): Buffer;
export function genesisHash(genesis: Bytes): Buffer;
export function engineIds(): string[];
export function stateModelIds(): string[];
export function proposerIds(): string[];
/** A BLS12-381 validator key derived from at least 32 bytes of key material. */
export function newBLSValidator(keyMaterial: Bytes): KeyPair;
/** Assembles a chain from a genesis; null when the mixture is unknown, mismatched or forbidden by the posture. */
export function assembleChain(genesis: Bytes, me: number, blsPublic: Bytes, blsSecret: Bytes): Chain | null;

/** A consensus engine written in JavaScript. */
export interface ForeignEngine {
  /** The justification for a block this node proposes, or null to refuse. */
  justify(block: Buffer): Bytes | null;
  verify(block: Buffer, justification: Buffer): boolean;
}
export function registerEngine(id: string, engine: ForeignEngine, grindProof?: boolean): boolean;
/** policy(height, validatorCount) returns the proposer index. */
export function registerProposer(id: string, policy: (height: number, validatorCount: number) => number): boolean;
/** Largest justification a foreign engine may return, in bytes. */
export const MAX_JUSTIFICATION: number;

/** An assembled chain behind the uniform propose, seal or mine, submit flow. */
export class Chain {
  readonly height: number;
  /** The chain id this chain runs. */
  readonly chainId: number;
  /** Transactions waiting for the next proposal. */
  readonly pendingCount: number;
  blueprint(): Buffer;
  /** The next block, carrying every queued transaction. */
  propose(timestamp: Int): Buffer;
  /** PoS and VRF justification; empty for engines that do not seal. */
  seal(block: Bytes): Buffer;
  /** PoW: the solved block. */
  mine(block: Bytes): Buffer;
  /** BFT: this validator's precommit as a commit certificate. */
  certify(block: Bytes): Buffer;
  submit(block: Bytes, justification?: Bytes): boolean;
  stateRoot(): Buffer;
  /** Queues a signed, encoded transaction for the next proposal after validating it; returns the txid. */
  addTx(tx: Bytes): Buffer;
  /** BFT: schedules the next validator set; the next proposal announces it. */
  scheduleValidators(validators: ValidatorEntry[]): void;
  validators(): ValidatorInfo[];
  /** Parent chains: the next block anchoring encoded anchors in ascending child id order. */
  proposeAnchored(timestamp: Int, anchors: Bytes[]): Buffer;
  childTip(childChainId: Int): Buffer;
  subchainHop(childChainId: Int): Buffer;
  /** Account model chains: a state inclusion proof for an address. */
  proveAccount(address: Bytes): Buffer;
  /** Record model chains: signs a write with a 64 byte Ed25519 secret, nonce from the chain, and queues it. */
  write(secret: Bytes, key: Bytes, op: RecordOpValue, value?: Bytes): Buffer;
  /** Record model chains: signs with the key store account at from and queues it. */
  writeFromStore(store: KeyStore, from: Bytes, key: Bytes, op: RecordOpValue, value?: Bytes): Buffer;
  /** Record model chains: the committed record at key, or null. */
  record(key: Bytes): RecordEntry | null;
  recordNonce(address: Bytes): number;
  proveRecord(key: Bytes): Buffer;
  /** Buffered block events as a JSON array; empty while onEvent is set. */
  drainEvents(): string;
  onEvent(cb: ((json: string) => void) | null): void;
  /** Prometheus text: height, blocks finalized and rejected, transactions applied. */
  metrics(): string;
  free(): void;
}

// -- Records -------------------------------------------------------------------

export type RecordOpValue = 0 | 1 | 2;
export const RecordOp: { readonly PUT: 0; readonly TRANSFER: 1; readonly DELETE: 2 };
export interface RecordEntry { owner: Buffer; version: number; value: Buffer }
export interface RecordLeaf extends RecordEntry { key: Buffer }
export function signRecordTx(opts: { chainId: Int; nonce: Int; key: Bytes; op: RecordOpValue; value?: Bytes; secret: Bytes }): Buffer;
export function addressOf(publicKey: Bytes): Buffer;
/** The record a verified state proof commits to. */
export function recordFromProof(proof: Bytes): RecordLeaf;
/** A record write signed by a hybrid Ed25519 + ML-DSA-65 account. */
export function signRecordTxHybrid(opts: { chainId: Int; nonce: Int; key: Bytes; op: RecordOpValue; value?: Bytes; publicKey: Bytes; secret: Bytes }): Buffer;

// -- Light client ---------------------------------------------------------------

export interface Checkpoint { header: Bytes; certificate: Bytes; validators: ValidatorEntry[] }
export const SkipRefused: 0;
export const SkipAccepted: 1;
export const SkipInsufficientOverlap: 2;
export class LightClient {
  constructor(chainId: Int, validators: ValidatorEntry[], trustedHeader: Bytes);
  readonly height: number;
  tip(): Buffer;
  validators(): ValidatorInfo[];
  setTrustLevel(numerator: number, denominator: number): void;
  setMaxSkip(blocks: Int): void;
  accept(header: Bytes, certificate: Bytes): boolean;
  /** SkipRefused, SkipAccepted or SkipInsufficientOverlap. */
  acceptSkipping(checkpoint: Checkpoint): number;
  /** Bisects towards target, fetching intermediate checkpoints on demand. Throws on a refusal. */
  syncTo(target: Checkpoint, targetHeight: number, fetch: (height: number) => Checkpoint | null): void;
  free(): void;
}

// -- Subchains and chain trees ----------------------------------------------

export function genesisHeader(genesis: Bytes): Buffer;
export function encodeChildRegistration(opts: { chainId: Int; genesisHeader: Bytes; validators: ValidatorEntry[]; maxDepth?: number }): Buffer;
export function encodeSubchainPolicy(opts: { maxChildren: number; maxDepth?: number; maxAnchorsPerBlock?: number; maxHeadersPerAnchor?: number; children?: Bytes[] }): Buffer;
export function commitSubchainPolicy(genesis: Bytes, policy: Bytes): Buffer;
export function commitParentBinding(genesis: Bytes, parentChainId: Int, parentGenesisHash: Bytes): Buffer;
export function blockHeader(block: Bytes): Buffer;
export function headerSubchainRoot(header: Bytes): Buffer;
export function certifiedHeader(header: Bytes, certificate: Bytes): Buffer;
export function encodeAnchor(childChainId: Int, certifiedHeaders: Bytes[]): Buffer;
export function encodeSubchainProof(hops: Bytes[], stateProof: Bytes): Buffer;
export function verifySubchainProof(parentSubchainRoot: Bytes, proof: Bytes): boolean;

export interface TreeValidator { publicKey: Bytes; secret?: Bytes | null; power: Int }
export interface TreeNode {
  chainId: Int;
  blueprint?: Blueprint;
  assuranceLevel?: number;
  accounts?: GenesisAccount[];
  validators?: TreeValidator[];
  maxAnchorsPerBlock?: number;
  maxHeadersPerAnchor?: number;
  children?: TreeNode[];
}
export function encodeTreeNode(node: TreeNode): Buffer;
/** Why a spec is structurally invalid; empty when valid. */
export function chainTreeSpecError(spec: Bytes): string;
/** A family of chains built and produced together. */
export class ChainTree {
  constructor(root: TreeNode);
  chainIds(): number[];
  height(chainId: Int): number;
  genesis(chainId: Int): Buffer;
  tip(chainId: Int): Buffer;
  pendingAnchors(chainId: Int): number;
  produce(chainId: Int, timestamp: Int): Buffer;
  proveAccount(chainId: Int, address: Bytes): Buffer;
  verify(proof: Bytes): boolean;
  onFinalized(cb: (chainId: number, block: Buffer) => void): void;
  txSource(cb: (chainId: number, timestamp: number) => Bytes[]): void;
  free(): void;
}

// -- Custody: threshold and portable accounts --------------------------------

/** Creates a t of n threshold account; returns one share card per participant. */
export function createThresholdAccount(n: number, t: number): Buffer[];
export function shareInfo(card: Bytes): { index: number; groupPublicKey: Buffer; publicShare: Buffer };
/** Round one of a threshold signature: keep the nonces, share the commitment. */
export function commit(card: Bytes): { nonces: Buffer; commitment: Buffer };
export function sealShare(card: Bytes, passphrase: string, ops?: Int, mem?: number): Buffer;
export function unsealShare(envelope: Bytes, passphrase: string): Buffer;
export class Ceremony {
  constructor(groupPublicKey: Bytes, message: Bytes, commitments: Bytes[]);
  sign(card: Bytes, nonces: Bytes): Buffer;
  verifyShare(id: number, publicShare: Bytes, sigShare: Bytes): boolean;
  /** The aggregated Ed25519 signature. */
  aggregate(ids: number[], sigShares: Bytes[]): Buffer;
  free(): void;
}
export function hybridKemKeypair(): KeyPair;
export function sealAccountPassphrase(publicKey: Bytes, secret: Bytes, passphrase: string, ops?: Int, mem?: number): Buffer;
export function openAccountPassphrase(envelope: Bytes, passphrase: string): KeyPair;
export function sealAccountTo(publicKey: Bytes, secret: Bytes, recipientKemPublic: Bytes): Buffer;
export function openAccountTo(envelope: Bytes, recipientKemSecret: Bytes): KeyPair;
export function sealAccountSignedTo(publicKey: Bytes, secret: Bytes, recipientKemPublic: Bytes, senderSigSecret: Bytes): Buffer;
export function openAccountSignedFrom(envelope: Bytes, recipientKemSecret: Bytes, senderSigPublic: Bytes): KeyPair;

// -- Assurance, sealed mempools and credentials ------------------------------

export function describeAssurance(level: number): { ratcheted: boolean; vrf: boolean; thresholdCustody: boolean; sealedMempool: boolean; sigAlgos: number[] };
/** Encodes a transaction from its fields. */
export function encodeTx(opts: { version?: number; chainId?: Int; sigAlgo?: number; fee?: Int; body: Bytes; author: Bytes; signature: Bytes }): Buffer;
/** Seals an encoded transaction to the proposer's hybrid KEM public key. */
export function sealTx(txEncoded: Bytes, proposerKemPublic: Bytes): Buffer;
export class SealedMempool {
  constructor(capacity?: number);
  readonly size: number;
  add(sealedTx: Bytes): boolean;
  /** The proposer opens up to max sealed transactions, highest fee first. */
  reveal(proposerKemSecret: Bytes, max: number): Buffer[];
  free(): void;
}
export function newBBSIssuer(keyMaterial: Bytes): KeyPair;
export const CredentialModePseudonymous: 1;
export class CredentialPolicy {
  constructor(chainId: Int, issuerPublic: Bytes, header: Bytes, attributeCount?: number, minTier?: number, mode?: number);
  readonly encoded: Buffer;
  hash(): Buffer;
  issue(issuerSecret: Bytes, memberSecret: Bytes, tier: number, notAfter: Int, issuerEpoch: number): Buffer;
  present(credential: Bytes, epoch: number, scope: string, challenge: Bytes): Buffer;
  membershipCommit(memberSecret: Bytes): Buffer;
  blindIssue(issuerSecret: Bytes, commitment: Bytes, tier: number, notAfter: Int, issuerEpoch: number): Buffer;
  presentPseudonymous(credential: Bytes, epoch: number, scope: string, challenge: Bytes): Buffer;
  /** The answer to an admission challenge wire from a peer holding credential. */
  admissionAnswer(credential: Bytes, wire: Bytes): Buffer;
}
export function extractCommitment(membershipCommit: Bytes): Buffer;
export function finalizeMembership(membershipCommit: Bytes, blindIssuance: Bytes): Buffer;
export class CredentialGate {
  constructor(policy: CredentialPolicy);
  admit(epoch: number, scope: string, challenge: Bytes, presentation: Bytes, now: Int): boolean;
  /** The 48 byte pseudonym on admission, or null. */
  admitPseudonymous(epoch: number, scope: string, challenge: Bytes, presentation: Bytes, now: Int): Buffer | null;
  revokeIssuerEpoch(epoch: number): void;
  revokeNym(nym: Bytes): void;
  /** Records an admission; false when this nym was already admitted for the epoch and scope. */
  noteAdmission(epoch: number, scope: string, nym: Bytes): boolean;
  /** A fresh admission challenge: keep nonce, send wire. */
  admissionChallenge(epoch: number): { nonce: Buffer; wire: Buffer };
  admissionGranted(epoch: number, nonce: Bytes, presentation: Bytes, now: Int): boolean;
  free(): void;
}

// -- Peer records, address books, Noise sessions, PeerMesh ---------------------

export function signPeerRecord(opts: { id: number; host: string; port: number; identity: SecureIdentity; prekey?: Bytes }): Buffer;
export function verifyPeerRecord(record: Bytes): boolean;
export interface PeerRecordInfo { id: number; host: string; port: number; kemPublic: Buffer; sigPublic: Buffer; prekey: Buffer; signature: Buffer }
export function readPeerRecord(record: Bytes): PeerRecordInfo;
/** The peer records a node knows; merge verifies every record and never overwrites a known id. */
export class AddressBook {
  constructor(encoded?: Bytes | null);
  readonly size: number;
  add(record: Bytes): void;
  merge(encoded: Bytes): number;
  record(id: number): Buffer | null;
  contains(id: number): boolean;
  records(): Buffer[];
  encode(): Buffer;
  free(): void;
}

export interface NoiseIdentity { noiseStatic: KeyPair; sig: KeyPair }
export function generateNoiseIdentity(): NoiseIdentity;
/** A Noise XX link with a post quantum identity proof; confidentiality is X25519. Pin peerIdentity. */
export class NoiseSession {
  static initiate(me: NoiseIdentity, prologue?: Bytes): NoiseSession;
  static accept(me: NoiseIdentity, msg0: Bytes, prologue?: Bytes): NoiseSession;
  readonly handshakeMessage: Buffer;
  completeInitiator(msg1: Bytes): Buffer;
  completeResponder(msg2: Bytes): void;
  identityProof(): Buffer;
  verifyIdentityProof(proof: Bytes): void;
  readonly open: boolean;
  readonly peerIdentity: Buffer;
  readonly peerStatic: Buffer;
  readonly handshakeHash: Buffer;
  encrypt(plaintext: Bytes): Buffer;
  decrypt(record: Bytes): Buffer;
  free(): void;
}
export function initiateNoiseSession(me: NoiseIdentity, prologue?: Bytes): NoiseSession;
export function acceptNoiseSession(me: NoiseIdentity, msg0: Bytes, prologue?: Bytes): NoiseSession;

/** A deterministic in-memory mesh of NetNodes; a node's index is its mesh id. */
export class PeerMesh {
  constructor(nodes?: NetNode[]);
  nodes: NetNode[];
  link(a: number, b: number): void;
  linkAll(): void;
  apply(node: number, out: Outbound): void;
  start(): void;
  advance(): void;
  deliver(): boolean;
  step(): boolean;
  run(until: () => boolean, budget?: number): boolean;
  /** Runs until the validators (all by default) committed, then drains the relays. */
  settle(validators?: number[]): boolean;
  free(): void;
}

