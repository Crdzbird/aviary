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
}

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
