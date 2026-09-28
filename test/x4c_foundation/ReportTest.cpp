#include <cassert>
#include <cstdio>
#include <string>

#include "util/X4cDiagnostics.h"

namespace freeink {
XteinkDisplayProbeDiag testProbe;
const XteinkDisplayProbeDiag& getXteinkDisplayProbeDiag() { return testProbe; }
}  // namespace freeink

struct Report {
  std::string text;
  void printf(const char* value) { text += value; }
  template <typename... Args>
  void printf(const char* format, Args... args) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), format, args...);
    text += buffer;
  }
  bool has(const char* value) const { return text.find(value) != std::string::npos; }
};

int main() {
  // Use the pinned SDK's real X4C profile, not a second copy of its pinout.
  static_assert(BoardConfig::XTEINK_X4_CLASSIC.batteryAdc == -1);
  static_assert(BoardConfig::XTEINK_X4_CLASSIC.input.up == 0);
  static_assert(BoardConfig::XTEINK_X4_CLASSIC.batteryGauge.gaugeType == BoardConfig::GaugeType::Cw2017);
  Report nvs;
  writeX4cHardwareDiagnostics(nvs);
  assert(nvs.has("display_controller_source=NVS\n"));
  assert(!nvs.has("display_probe_ver="));
  assert(nvs.has("battery_i2c_address=0x63\n"));
  assert(nvs.has("rtc_i2c_address=0x51\n"));
  assert(nvs.has("sensor_i2c_sda=39\nsensor_i2c_scl=38\n"));
  assert(nvs.has("sdmmc_clk=41\nsdmmc_cmd=42\nsdmmc_d0=40\n"));
  assert(nvs.has("usb_detection_supported=false\n"));
  for (unsigned id : {0x01, 0x02, 0x68, 0x69, 0x00, 0xff, 0x03, 0x67}) {
    freeink::testProbe.valid = true;
    freeink::testProbe.ver[2] = id;
    freeink::testProbe.busyTimedOut = id == 0xff;
    BoardConfig::ACTIVE.displayControllerVariant = id;
    Report ver;
    writeX4cHardwareDiagnostics(ver);
    assert(ver.has("display_controller_source=VER\n"));
    const bool recognized = id == 1 || id == 2 || id == 0x68 || id == 0x69;
    assert(ver.has(recognized ? "display_probe_recognized=true\n" : "display_probe_recognized=false\n"));
    assert(ver.has(id == 0xff ? "display_probe_busy_timeout=true\n" : "display_probe_busy_timeout=false\n"));
  }
  puts("PASS: X4C report with real profile, NVS/VER/unknown ID/timeout and unknown USB");
}
