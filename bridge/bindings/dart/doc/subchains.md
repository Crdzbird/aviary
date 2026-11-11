# Subchains and trees

Subchains anchored into a parent, cross chain proofs from the parent header, and chain trees that produce a whole hierarchy in rounds.

A parent with a dynamic policy registers children in any block through signed registry operations, keeps one record per child in a Merkle registry, and proves one child with its record and a short path however many children it holds. Chains and trees are saved with `snapshot` and brought back with `restoreChain` and `restoreChainTree`.
