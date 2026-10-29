# The Linux engine is not built here: aviary_dart's build hook bundles the
# prebuilt self-contained libaviary_c.so as a native code asset, and the Dart
# loader finds it next to the application. The plugin therefore bundles no
# library of its own.
cmake_minimum_required(VERSION 3.13)
project(aviary_lite_library VERSION 1.0.0 LANGUAGES CXX)
set(aviary_lite_bundled_libraries "" PARENT_SCOPE)
