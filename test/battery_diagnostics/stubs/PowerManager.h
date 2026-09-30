#pragma once
namespace freeink {
struct PowerManager {
  struct AbortedSleepInfo {
    bool aborted = false;
    int wakeupCause = 0;
    int wakePinLevel = 0;
  };
  static AbortedSleepInfo pendingAbort;
  static inline int takeCalls = 0;
  static AbortedSleepInfo takeAbortedSleepInfo() {
    ++takeCalls;
    const auto info = pendingAbort;
    pendingAbort = {};
    return info;
  }
  static void powerDownRailsForSleep() {}
  static void deepSleepUntilPowerButton() {}
};
inline PowerManager::AbortedSleepInfo PowerManager::pendingAbort;
}  // namespace freeink
