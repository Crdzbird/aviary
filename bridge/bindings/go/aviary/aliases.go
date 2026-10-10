package aviary

// Cross-binding naming aliases. Each name below is the one used by the
// Dart/Flutter binding; it forwards to the Go-idiomatic name so consumers can
// use either vocabulary interchangeably. (The reverse aliases live in the Dart
// binding.) These are the only names that differed between the two bindings.

// CreateNode is an alias for [NewNode] (Dart/Flutter: createNode).
func CreateNode(chainID uint64) *Node { return NewNode(chainID) }

// CryptoBackend is an alias for [CryptoBackendVersion] (Dart/Flutter: cryptoBackend).
func CryptoBackend() string { return CryptoBackendVersion() }

// EnsureInitialized is an alias for [Init] (Dart/Flutter: ensureInitialized).
func EnsureInitialized() error { return Init() }

// IsInitialized is an alias for [Initialized] (Dart/Flutter: isInitialized).
func IsInitialized() bool { return Initialized() }

// Dispose is an alias for [Node.Free] (Dart/Flutter: dispose).
func (n *Node) Dispose() { n.Free() }
