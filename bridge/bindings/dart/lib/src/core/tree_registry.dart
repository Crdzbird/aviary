part of '../../aviary.dart';

final Map<int, ChainTree> _trees = {};
int _nextTreeHandle = 1;

void _treeFinTrampoline(
    Pointer<Void> ctx, int chainId, Pointer<Uint8> block, int len) {
  final cb = _trees[ctx.address]?.onFinalized;
  if (cb == null) return;
  cb(chainId, Uint8List.fromList(block.asTypedList(len)));
}

int _treeTxTrampoline(Pointer<Void> ctx, int chainId, int ts,
    Pointer<Uint8> out, int cap, Pointer<Size> outLen) {
  final src = _trees[ctx.address]?.txSource;
  if (src == null) return 0;
  final blob = _blobList(src(chainId, ts));
  if (blob.length > cap) return 0;
  out.asTypedList(cap).setAll(0, blob);
  outLen.value = blob.length;
  return 1;
}
