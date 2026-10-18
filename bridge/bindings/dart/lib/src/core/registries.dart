part of '../../aviary.dart';

final Map<int, ChainTree> _trees = {};
int _nextTreeHandle = 1;

void _treeFinTrampoline(
  Pointer<Void> ctx,
  int chainId,
  Pointer<Uint8> block,
  int len,
) {
  final cb = _trees[ctx.address]?.onFinalized;
  if (cb == null) return;
  cb(chainId, Uint8List.fromList(block.asTypedList(len)));
}

int _treeTxTrampoline(
  Pointer<Void> ctx,
  int chainId,
  int ts,
  Pointer<Uint8> out,
  int cap,
  Pointer<Size> outLen,
) {
  final src = _trees[ctx.address]?.txSource;
  if (src == null) return 0;
  final blob = _blobList(src(chainId, ts));
  if (blob.length > cap) return 0;
  out.asTypedList(cap).setAll(0, blob);
  outLen.value = blob.length;
  return 1;
}

final Map<int, ForeignEngine> _foreignEngines = {};
final Map<int, int Function(int height, int validators)> _foreignProposers = {};
int _nextForeignHandle = 1;
final List<NativeCallable<Function>> _foreignCallables = [];

int _justifyTrampoline(
  Pointer<Void> ctx,
  Pointer<Uint8> block,
  int len,
  Pointer<Uint8> out,
  int cap,
  Pointer<Size> outLen,
) {
  final e = _foreignEngines[ctx.address];
  if (e == null) return 0;
  final j = e.justify(Uint8List.fromList(block.asTypedList(len)));
  if (j == null || j.length > cap) return 0;
  out.asTypedList(cap).setAll(0, j);
  outLen.value = j.length;
  return 1;
}

int _verifyTrampoline(
  Pointer<Void> ctx,
  Pointer<Uint8> block,
  int len,
  Pointer<Uint8> just,
  int jlen,
) {
  final e = _foreignEngines[ctx.address];
  if (e == null) return 0;
  final ok = e.verify(
    Uint8List.fromList(block.asTypedList(len)),
    Uint8List.fromList(just.asTypedList(jlen)),
  );
  return ok ? 1 : 0;
}

int _proposerTrampoline(Pointer<Void> ctx, int height, int validators) {
  final p = _foreignProposers[ctx.address];
  if (p == null) return 0;
  return p(height, validators);
}
