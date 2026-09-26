#include <cassert>
#include <cstdint>

#include "DecodeCancellation.h"
#include "PixelCache.h"
struct Input {
  int state = 0, physical = 0, press = 0, release = 0, samples = 0;
  void update() {
    ++samples;
    press = physical & ~state;
    release = state & ~physical;
    state = physical;
  }
};
class HalGPIO {
 public:
  static constexpr uint8_t BTN_POWER = 6;
  Input inputMgr;
  bool deferInputUpdate = false, usbStateChanged = false, lastUsbConnected = false, usb = false;
  bool isUsbConnected() const { return usb; }
  bool wasAnyPressed() const { return inputMgr.press; }
  bool wasAnyReleased() const { return inputMgr.release; }
  bool wasUsbStateChanged() const { return usbStateChanged; }
  bool isPressed(uint8_t b) const { return inputMgr.state & (1 << b); }
  void update();
  bool pollIdleInput();
};
// Generated from the production HalGPIO.cpp by the test runner.
#include "HalGPIOIdleMethods.inc"
int main() {
  HalGPIO g;
  assert(!g.pollIdleInput());
  g.inputMgr.physical = 4;
  DecodeCancellation c;
  c.context = &g;
  c.requested = [](void* p) { return static_cast<HalGPIO*>(p)->pollIdleInput(); };
  PixelCache cache;
  assert(cache.begin("partial", 8, 4, 0, 0, 1));
  assert(cache.advanceTo(2));
  assert(c.poll());
  cache.abort();
  assert(!files.count("partial") && cache.buffer == nullptr);
  // A release before normal processing must not erase the already captured press.
  g.inputMgr.physical = 0;
  const int samples = g.inputMgr.samples;
  assert(c.poll() && g.inputMgr.samples == samples);
  g.update();
  assert(g.wasAnyPressed() && g.inputMgr.samples == samples);
  g.update();
  assert(!g.wasAnyPressed() && g.wasAnyReleased());
  g.update();
  assert(!g.wasAnyReleased());
  // USB cancellation is handed off once as well.
  g.usb = true;
  assert(g.pollIdleInput());
  g.update();
  assert(g.wasUsbStateChanged());
  g.update();
  assert(!g.wasUsbStateChanged());
  assert(cache.begin("partial", 8, 4, 0, 0, 1));
  assert(cache.finalize());
  assert(files.count("partial"));
}
