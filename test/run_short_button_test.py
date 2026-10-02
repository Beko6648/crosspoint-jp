#!/usr/bin/env python3
"""Check extracted idle wait/contact code against the SDK debounce algorithm.

Time and ADC readings are controlled fixtures, not measurements of device timing
or battery draw. The old 50ms wait remains the comparison for missed short taps.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
hal = (root / 'lib/hal/HalGPIO.cpp').read_text(encoding='utf-8')
main = (root / 'src/main.cpp').read_text(encoding='utf-8')
sdk = (root / 'freeink-sdk/libs/hardware/InputManager/src/InputManager.cpp').read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


start = main.index('      const unsigned long idleStart = millis();')
end = main.index('#endif', start)
wait = main[start:end]
start = sdk.index('  const uint8_t state = getState();', sdk.index('void InputManager::update()'))
end = sdk.index('\n}\n', start)
debounce = sdk[start:end]

source = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
unsigned long clockMs = 0, pressAt = 0, releaseAt = 0;
bool physicalMode = false;
unsigned long millis() { return clockMs; }
void delay(unsigned long ms) { clockMs += ms; }
bool contact() { return physicalMode && clockMs >= pressAt && clockMs < releaseAt; }
struct InputManager {
 struct ButtonAdcSample { int pin = -1, raw = -1, button = -1; };
 int raw1 = 4095, raw2 = 4095, reads = 0;
 bool power = false;
 uint8_t lastState = 0, currentState = 0;
 unsigned long lastDebounceTime = 0;
 static constexpr unsigned long DEBOUNCE_DELAY = 5;
 int presses = 0, releases = 0;
 bool isPowerButtonPhysicallyPressed() const { return power; }
 void readButtonAdc(ButtonAdcSample& a, ButtonAdcSample& b) {
   ++reads; a.raw = contact() ? 1000 : raw1; b.raw = raw2;
 }
 uint8_t getState() const { return contact() ? 1 : 0; }
 void applyStateChange(uint8_t state, unsigned long) {
   if (state) ++presses; else ++releases;
   currentState = state;
 }
 void update();
};
struct HalGPIO { InputManager inputMgr; bool rawInputActive(); } gpio;
''' + function(hal, 'bool HalGPIO::rawInputActive()') + '\nvoid idleWait() {\n' + wait + '\n}\n' + '\nvoid InputManager::update() {\nconst unsigned long currentTime = millis();\n' + debounce + '\n}\n' + r'''
int main() {
 unsigned checks = 0, oldMisses = 0, newMisses = 0;
 auto check = [&](bool ok) { ++checks; assert(ok); };
 // Boundaries, missing ADC ladders, power polarity handled by the SDK.
 physicalMode = false;
 for (int a : {-1, 0, 750, 1120, 2090, 3100, 3899, 3999, 4000, 4095}) {
  for (int b : {-1, 0, 750, 1120, 2090, 3100, 3899, 3999, 4000, 4095}) {
   for (bool power : {false, true}) {
    gpio.inputMgr = {}; gpio.inputMgr.raw1 = a; gpio.inputMgr.raw2 = b; gpio.inputMgr.power = power;
    check(gpio.rawInputActive() == (power || (a >= 0 && a < 4000) || (b >= 0 && b < 4000)));
    check(gpio.inputMgr.reads == (power ? 0 : 1));
    check(gpio.inputMgr.presses == 0 && gpio.inputMgr.releases == 0 && gpio.inputMgr.currentState == 0);
   }
  }
 }
 // Quiet idle still waits 50ms; contact interrupts on the first 10ms slice.
 gpio.inputMgr = {}; clockMs = 0; idleWait(); check(clockMs == 50 && gpio.inputMgr.reads == 5);
 gpio.inputMgr.raw2 = 1120; clockMs = 0; idleWait(); check(clockMs == 10);
 // Unsigned time subtraction continues across millis rollover.
 gpio.inputMgr = {}; clockMs = std::numeric_limits<unsigned long>::max() - 24;
 const auto before = clockMs; idleWait(); check(clockMs - before == 50);
 // Sweep tap phase, duration and per-loop workload using actual SDK debounce.
 for (int cost : {0, 5, 15}) {
  for (int duration : {60, 70, 90, 120, 200, 1500}) {
   for (int phase = 0; phase < 65; ++phase) {
    for (bool old : {true, false}) {
     gpio.inputMgr = {}; clockMs = 0; pressAt = 100 + phase; releaseAt = pressAt + duration;
     physicalMode = true;
     bool active = false;
     while (clockMs < releaseAt + 150) {
      gpio.inputMgr.update();
      if (gpio.inputMgr.presses || gpio.inputMgr.releases) active = true;
      delay(cost);
      if (active) delay(10);
      else if (old) delay(50);
      else idleWait();
     }
     check(gpio.inputMgr.presses <= 1 && gpio.inputMgr.releases <= 1);
     if (old) oldMisses += gpio.inputMgr.presses == 0;
     else {
      newMisses += gpio.inputMgr.presses == 0;
      check(gpio.inputMgr.presses == 1 && gpio.inputMgr.releases == 1);
     }
    }
   }
  }
 }
 // Brief noise may wake the loop but must not bypass debounce into an event.
 for (int phase = 0; phase < 65; ++phase) {
  gpio.inputMgr = {}; clockMs = 0; pressAt = 100 + phase; releaseAt = pressAt + 1;
  while (clockMs < 300) { gpio.inputMgr.update(); delay(15); idleWait(); }
  check(gpio.inputMgr.presses == 0 && gpio.inputMgr.releases == 0);
 }
 check(oldMisses > 0 && newMisses == 0);
 std::cout << "PASS checks=" << checks << " old missed taps=" << oldMisses
           << " new missed taps=" << newMisses << " (controlled timing)\n";
}
'''
with tempfile.TemporaryDirectory(prefix='yomuka-short-button-') as directory:
    path = Path(directory)
    cpp = path / 'test.cpp'
    cpp.write_text(source, encoding='utf-8')
    exe = path / ('test.exe' if os.name == 'nt' else 'test')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
