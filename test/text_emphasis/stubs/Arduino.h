#pragma once
#include <cstdint>
#include <cstdlib>
struct TestEsp {
  uint32_t getFreeHeap() const { return 1024 * 1024; }
};
inline TestEsp ESP;
