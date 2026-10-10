/// blockchainer is an FFI plugin: there is no method channel and no plugin class.
/// Importing the native module here keeps the BlockchainerC binary linked into
/// the app so dart:ffi can resolve its symbols from the process image.
import BlockchainerC

public enum BlockchainerPlugin {}
