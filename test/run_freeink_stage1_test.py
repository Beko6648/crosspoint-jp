#!/usr/bin/env python3
"""Exercise the pinned SDK detector with its GPIO harness and real profiles."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "freeink-sdk"
hardware = sdk / "libs/hardware"
detector = hardware / "XteinkDetect"
host = detector / "test/host"
compiler = os.environ.get("CXX", "c++")

with tempfile.TemporaryDirectory(prefix="yomuka-stage1-") as folder:
    out = Path(folder)
    common = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
              "-I" + str(out), "-I" + str(host / "stubs"),
              "-I" + str(hardware / "BoardConfig/include"), "-I" + str(detector / "include")]
    # First run the unchanged upstream C3 protocol regression.
    exe = out / "x3-probe"
    subprocess.run(common + ["-DFREEINK_DEVICE_X3=1", "-DFREEINK_DEVICE_X4=1",
                   str(host / "test_display_probe.cpp"), str(detector / "src/XteinkDetect.cpp"),
                   "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    # Reuse the SDK's wire-level emulator, replacing only its test cases and
    # initial board. No SDK source or submodule-local test file is modified.
    harness = (host / "test_display_probe.cpp").read_text().split("int main() {")[0]
    harness = harness.replace("BoardConfig::XTEINK_X3.display", "BoardConfig::ACTIVE.display")
    (out / "probe.cpp").write_text(harness + '''
#include <nvs.h>
int main() {
  using C = BoardConfig::DisplayController;
  using B = BoardConfig::Board;
  for (int st : {1, 0x0b, 2, 0x0c, 3, 0, 0xff}) {
    reset(B::XteinkX4Classic, {0, 0, 0x01});
    testScreenType = st;
    freeink::applyXteinkDisplayController();
    const C expected = (st == 1 || st == 0x0b) ? C::UC8179 :
                       (st == 2 || st == 0x0c) ? C::UC8279 : C::SSD1677;
    assert(BoardConfig::ACTIVE.displayController == expected);
    assert(commands.empty() && !freeink::getXteinkDisplayProbeDiag().valid);
  }
  testScreenType = -1;
  for (int id : {0x01, 0x02, 0x68, 0x69, 0x00, 0xff, 0x03, 0x67}) {
    reset(B::XteinkX4Classic, {0, 0, static_cast<uint8_t>(id)});
    freeink::applyXteinkDisplayController();
    assert(BoardConfig::ACTIVE.displayController == (id == 1 ? C::UC8179 : C::UC8279));
    checkX3Wire();
    const bool recognized = id == 1 || id == 2 || id == 0x68 || id == 0x69;
    assert(BoardConfig::ACTIVE.displayControllerVariant == (recognized ? id : 0));
  }
  reset(B::XteinkX4Classic, {0xff, 0xff, 0xff});
  readyAt = 99999;
  freeink::applyXteinkDisplayController();
  assert(freeink::getXteinkDisplayProbeDiag().busyTimedOut);
  assert(BoardConfig::ACTIVE.displayController == C::UC8279);
  puts("PASS: X4C NVS priority/no probe, VER IDs, unknown UC8279 fallback, bounded BUSY");
}
''')
    (out / "nvs.h").write_text('''#pragma once
#include <cstdint>
using nvs_handle_t = int;
using esp_err_t = int;
constexpr int NVS_READONLY = 0, ESP_OK = 0;
inline int testScreenType = -1;
inline int nvs_open(const char*, int, nvs_handle_t* h) { *h = 1; return ESP_OK; }
inline int nvs_get_u8(nvs_handle_t, const char*, uint8_t* value) {
  if (testScreenType < 0) return -1;
  *value = testScreenType; return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
''')
    exe = out / "x4c-probe"
    subprocess.run(common + ["-DFREEINK_DEVICE_X4CLASSIC=1", str(out / "probe.cpp"),
                   str(detector / "src/XteinkDetect.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    (out / "imu.cpp").write_text('''#include <BoardConfig.h>
static_assert(!BoardConfig::XTEINK_X3.sensors.imuSwapXY);
static_assert(!BoardConfig::XTEINK_X3.sensors.imuFlipX);
static_assert(!BoardConfig::XTEINK_X3.sensors.imuFlipY);
static_assert(!BoardConfig::XTEINK_X3_UC8279.sensors.imuSwapXY);
static_assert(!BoardConfig::XTEINK_X3_UC8279.sensors.imuFlipX);
static_assert(!BoardConfig::XTEINK_X3_UC8279.sensors.imuFlipY);
int main() {}
''')
    exe = out / "imu-profile"
    subprocess.run(common + ["-DFREEINK_DEVICE_X3=1", "-DFREEINK_DEVICE_X4=1",
                   str(out / "imu.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("PASS: both X3 profiles keep all IMU mount corrections OFF")
