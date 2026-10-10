// Type definitions for the Aviary Node.js binding.

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
  static accountAddress(publicKey: Buffer | Uint8Array): Buffer;
  credit(address: Buffer | Uint8Array, amount: number | bigint): void;
  submitTransfer(secret: Buffer | Uint8Array, toAddress: Buffer | Uint8Array, amount: number | bigint, nonce: number | bigint): Buffer;
  produceBlock(timestamp: number | bigint): number;
  height(): number;
  mempoolSize(): number;
  balance(address: Buffer | Uint8Array): number;
  nonce(address: Buffer | Uint8Array): number;
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
export function restoreNode(snapshot: Buffer | Uint8Array): Node;
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
