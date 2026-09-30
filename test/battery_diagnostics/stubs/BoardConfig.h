#pragma once
namespace BoardConfig {
inline struct {
  int batteryAdc = 0;
  struct {
    int latch0 = 13;
  } power;
} ACTIVE;
inline bool latchConflictsWithBus(int) { return false; }
}  // namespace BoardConfig
