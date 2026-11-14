part of '../aviary_native.dart';

int _jsonInt(Map<String, Object?> json, String key) {
  final v = json[key];
  if (v is int) return v;
  if (v is num) return v.toInt();
  return 0;
}

Uint8List _jsonHex(Map<String, Object?> json, String key) {
  final v = json[key];
  if (v is String) return v.hexBytes;
  return Uint8List(0);
}
