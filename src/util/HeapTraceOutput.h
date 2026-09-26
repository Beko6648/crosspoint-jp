#pragma once

#include <cstddef>
#include <cstdint>

namespace HeapTrace {
// A transient false readiness result must not consume the frame. Poll until
// ready or a bounded deadline; only the drain task waits, never allocator hooks.
template <typename Ready, typename Now, typename Yield>
bool waitForOutput(Ready ready, Now now, Yield yield, uint32_t timeoutMs) {
  const uint32_t started = now();
  for (;;) {
    if (ready()) return true;
    if (static_cast<uint32_t>(now() - started) >= timeoutMs) return false;
    yield();
  }
}
struct FrameWriteResult {
  bool connected;
  size_t written;
};

// HWCDC::write kicks the TX interrupt and drains the queue while holding its
// own writer lock. availableForWrite only observes it: waiting for a whole
// frame there can prevent the write that would restart a stalled queue.
// Invoke the driver once; a short result may already have queued data, so a
// blind retry could duplicate records.
template <typename Connected, typename Write, typename Now, typename Yield>
FrameWriteResult writeFrame(Connected connected, Write write, Now now, Yield yield, uint32_t timeoutMs) {
  if (!waitForOutput(connected, now, yield, timeoutMs)) return {false, 0};
  return {true, write()};
}
}  // namespace HeapTrace
