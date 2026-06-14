// Minimal zero-dependency test helper (matches cryptolib's convention).
#pragma once

#include <cstdio>
#include <exception>

namespace bctest {

inline int& failures() {
  static int f = 0;
  return f;
}

inline void check(bool cond, const char* what) {
  if (cond) {
    std::printf("  ok   %s\n", what);
  } else {
    std::printf("  FAIL %s\n", what);
    ++failures();
  }
}

template <class F>
bool throws(F&& f) {
  try {
    f();
    return false;
  } catch (...) {
    return true;
  }
}

inline int summary() {
  if (failures() == 0) {
    std::printf("\nALL PASS\n");
    return 0;
  }
  std::printf("\n%d FAILURE(S)\n", failures());
  return 1;
}

}  // namespace bctest

#define CHECK(cond) ::bctest::check((cond), #cond)
#define CHECK_MSG(cond, msg) ::bctest::check((cond), (msg))
#define CHECK_THROWS(expr) \
  ::bctest::check(::bctest::throws([&] { (void)(expr); }), "throws: " #expr)
