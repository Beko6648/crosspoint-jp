#!/usr/bin/env python3
"""Exercise the production loop scheduling tail with a contended render mutex."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
tail = main[main.index("  bool skipLoopDelay = false;"):]
locks = (root / "src/activities/ActivityManager.cpp").read_text(encoding="utf-8")
locks = locks[locks.index("RenderLock::RenderLock()"):locks.index("/**", locks.index("void RenderLock::unlock()"))]
source = r'''#include <cassert>
#include <cstdint>
#include "activities/RenderLock.h"
constexpr int pdTRUE=1;
constexpr unsigned portMAX_DELAY=9999;
bool held=false, hint=false;
int queried=0, powerCalls=0, delayMs=0, yields=0, gives=0;
bool saving=false;
bool rawInput=false;
struct HalGPIO {static constexpr uint8_t BTN_POWER=6;};
struct {bool rawInputActive(){return rawInput;}bool isPressed(uint8_t){return false;}} gpio;
unsigned lastActivityTime=0;
unsigned nowMs=200;
struct Manager {
 int renderingMutex=1;
 bool skipLoopDelay(){assert(held);++queried;return hint;}
} activityManager;
int xSemaphoreTake(int, unsigned ticks){assert(ticks==0 || ticks==portMAX_DELAY);if(held)return 0;held=true;return pdTRUE;}
void xSemaphoreGive(int){assert(held);held=false;++gives;}
struct HalPowerManager {static constexpr unsigned IDLE_POWER_SAVING_MS=100;};
struct Power {void setPowerSaving(bool value){assert(!held);++powerCalls;saving=value;}} powerManager;
unsigned millis(){return nowMs;}
void delay(int ms){delayMs+=ms;nowMs+=ms;}
void yield(){++yields;}
''' + locks + "\nvoid schedule(){\n" + tail + r'''
void reset(){held=false;hint=false;rawInput=false;queried=powerCalls=delayMs=yields=gives=0;lastActivityTime=0;nowMs=200;}
int main(){
 reset();held=true;schedule();assert(held&&queried==0&&powerCalls==0&&delayMs==10&&gives==0);
 reset();hint=true;schedule();assert(!held&&queried==1&&powerCalls==1&&!saving&&yields==1&&gives==1);
 reset();schedule();assert(!held&&saving&&powerCalls==1&&delayMs==50&&gives==1);
 reset();rawInput=true;schedule();assert(!held&&saving&&delayMs==10&&gives==1);
 reset();lastActivityTime=199;schedule();assert(!held&&powerCalls==0&&delayMs==10&&gives==1);
 reset();{RenderLock lock;assert(lock.ownsLock());}assert(!held&&gives==1);
}
'''
# Compile the actual prefetch entry guards up to the protected ready/state check.
reader = (root / "src/activities/reader/EpubReaderActivity.cpp").read_text(encoding="utf-8")
entries = ""
for name, ready in [("prefetchIdleChapter", "chapterRenderReady"), ("prefetchIdleImage", "idleRenderReady")]:
    start = reader.index("void EpubReaderActivity::" + name + "() {")
    end = reader.index("  if (" + ready + ".load() != ready", start)
    entries += reader[start:end] + "  assert(held); ++protectedWork;\n}\n"
source = source.replace("int main(){", "int schedulingCases(){").replace("reset();{RenderLock lock;assert(lock.ownsLock());}assert(!held&&gives==1);", "reset();{RenderLock lock;assert(lock.ownsLock());}assert(!held&&gives==1); return 0;")
source += r'''
#include <atomic>
#include <initializer_list>
#include <cstdint>
struct {bool tiltPageTurn=false;} SETTINGS;
struct EpubReaderActivity {
 std::atomic<uint32_t> chapterRenderReady{1},idleRenderReady{1};
 uint32_t chapterLastInput=0,idleLastInput=0;
 bool automaticPageTurnActive=false,rubyAdjustActive=false;
 int protectedWork=0;
 void prefetchIdleChapter(); void prefetchIdleImage();
};
''' + entries.replace("millis()", "3000u") + r'''
int main(){
 schedulingCases();
 for (auto fn : {&EpubReaderActivity::prefetchIdleChapter, &EpubReaderActivity::prefetchIdleImage}) {
  reset(); EpubReaderActivity reader;
  held=true;(reader.*fn)();assert(held&&gives==0&&reader.protectedWork==0);
  held=false;(reader.*fn)();assert(!held&&gives==1&&reader.protectedWork==1);
 }
}
'''
with tempfile.TemporaryDirectory(prefix="yomuka-scheduling-") as directory:
    path = Path(directory)
    cpp = path / "test.cpp"
    cpp.write_text(source, encoding="utf-8")
    exe = path / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "src"), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print("PASS: contended render skips hint/power policy; active, idle and recent paths; lock ownership")
