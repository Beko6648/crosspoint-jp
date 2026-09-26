#pragma once
#include <cstdint>
inline uint32_t nowMs = 10;
inline uint32_t millis() { return nowMs++; }
struct FakeEsp {
  uint32_t freeBytes=80000, maxAlloc=40000, minFree=50000;
  uint32_t getFreeHeap() const { return freeBytes; }
  uint32_t getMaxAllocHeap() const { return maxAlloc; }
  uint32_t getMinFreeHeap() const { return minFree; }
};
inline FakeEsp ESP;
