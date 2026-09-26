#include <cassert>
#include <cstdint>

#include "HeapTraceOutput.h"
int main() {
  uint32_t now = 0;
  int polls = 0;
  auto clock = [&] { return now; };
  auto tick = [&] { now += 10; };
  assert(HeapTrace::waitForOutput([] { return true; }, clock, tick, 250));
  assert(now == 0);
  // Transient connection/space failures preserve the frame until writable.
  assert(HeapTrace::waitForOutput([&] { return ++polls == 8; }, clock, tick, 250));
  assert(now == 70);
  now = 0;
  assert(!HeapTrace::waitForOutput([] { return false; }, clock, tick, 250));
  assert(now == 250);
  // millis() wrap must not bypass the deadline or hang forever.
  now = UINT32_MAX - 19;
  const auto start = now;
  assert(!HeapTrace::waitForOutput([] { return false; }, clock, tick, 250));
  assert(static_cast<uint32_t>(now - start) == 250);

  // Model the observed case: 62 bytes free, 194-byte frame. The queue only
  // resumes when the driver write kicks TX. A whole-frame precheck deadlocks.
  now = 0;
  size_t freeBytes = 62;
  int writes = 0;
  auto kickAndWrite = [&]() -> size_t {
    ++writes;
    freeBytes = 256;
    return 194;
  };
  assert(!HeapTrace::waitForOutput([&] { return freeBytes >= 194; }, clock, tick, 250));
  assert(writes == 0);
  now = 0;
  auto sent = HeapTrace::writeFrame([] { return true; }, kickAndWrite, clock, tick, 250);
  assert(sent.connected && sent.written == 194 && writes == 1 && now == 0);

  // The driver can also start TX with an entirely full queue.
  freeBytes = 0;
  sent = HeapTrace::writeFrame([] { return true; }, kickAndWrite, clock, tick, 250);
  assert(sent.written == 194 && writes == 2);

  // A partial write is surfaced, never retried and duplicated.
  writes = 0;
  sent = HeapTrace::writeFrame([] { return true; },
                               [&]() -> size_t {
                                 ++writes;
                                 return 62;
                               },
                               clock, tick, 250);
  assert(sent.connected && sent.written == 62 && writes == 1);
  now = 0;
  writes = 0;
  sent = HeapTrace::writeFrame([] { return false; }, kickAndWrite, clock, tick, 250);
  assert(!sent.connected && sent.written == 0 && writes == 0 && now == 250);

  now = 0;
  sent = HeapTrace::writeFrame([&] { return now >= 30; }, kickAndWrite, clock, tick, 250);
  assert(sent.connected && sent.written == 194 && writes == 1 && now == 30);
}
