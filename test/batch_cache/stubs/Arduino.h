#pragma once
#include <cstdint>
#include <cstdlib>
struct TestEsp {
  int calls = 0;
  int failAt = -1;
  uint32_t getFreeHeap() { return calls++ == failAt ? 0 : 1024 * 1024; }
};
inline TestEsp ESP;
