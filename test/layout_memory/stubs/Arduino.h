#pragma once
#include <cstdint>
#include <cstdlib>
struct TestEsp {
  uint32_t free = 1024 * 1024;
  uint32_t largest = 1024 * 1024;
  int checks = 0;
  int failAt = -1;
  uint32_t getFreeHeap() { return checks++ == failAt ? 0 : free; }
  uint32_t getMaxAllocHeap() const { return largest; }
};
inline TestEsp ESP;
