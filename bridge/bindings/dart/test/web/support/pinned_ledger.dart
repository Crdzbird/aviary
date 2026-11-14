/// Lines of vectors/hash_blake3.txt, block_vectors.txt and proof_vectors.txt,
/// copied for the browser, which cannot read the repository. The VM test
/// pinned_web_vectors_test.dart checks every entry against those files.
/// Hash entries are keyed `hash:<input hex>`, with `hash:` for empty input.
const pinnedLedger = <String, String>{
  'hash:': 'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262',
  'hash:617669617279':
      '6345ac5b1229b0a0eba19d679ca4359442f623f4f6a1cf823d5b9c280f289b2b',
  'txid': 'be99a206b50a87b74e8359ab3a2895ed2f082169089b7ff9ec416d9df18d36dd',
  'header_hash':
      '0ad7e2e233a9ebf827717d33b72a685eb993d8bac70b3cee517ba8023dd79793',
  'state_root':
      '9918413a2ad25fc3ec37457f522bcf091f0aaefc14af15d56b3cb2100632bce3',
  'state_proof':
      '01000000381111111111111111111111111111111111111111111111111111111111111111000000000000000000000000000001f40000000000000000000000020000000000000001bb53fe611991a376f327f60748faf0ceaae4671df4d5fb7a89b8f56cbce17033',
};
