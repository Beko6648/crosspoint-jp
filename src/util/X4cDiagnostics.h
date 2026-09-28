#pragma once

#include <BoardConfig.h>
#include <XteinkDetect.h>

// Call after HalGPIO::begin(), which applies the controller exactly once.
// At SDK 18e73e16, an X4C probe runs only when hw_calib/screenType is absent.
// Persist the snapshot without probing the live display bus or reading NVS again.
template <typename Report>
void writeX4cHardwareDiagnostics(Report& report) {
  const auto& board = BoardConfig::ACTIVE;
  const auto& probe = freeink::getXteinkDisplayProbeDiag();
  report.printf("display_controller_source=%s\n", probe.valid ? "VER" : "NVS");
  report.printf("display_controller_variant=0x%02X\n", board.displayControllerVariant);
  if (probe.valid) {
    const auto id = probe.ver[2];
    const bool recognized = id == 0x01 || id == 0x02 || id == 0x68 || id == 0x69;
    report.printf("display_probe_ver=%02X %02X %02X\n", probe.ver[0], probe.ver[1], id);
    report.printf("display_probe_recognized=%s\n", recognized ? "true" : "false");
    report.printf("display_probe_busy_timeout=%s\n", probe.busyTimedOut ? "true" : "false");
  }
  report.printf("battery_backend=cw2017\n");
  report.printf("battery_i2c_address=0x%02X\n", board.batteryGauge.gaugeAddr);
  report.printf("rtc_i2c_address=0x%02X\n", board.sensors.rtcAddr);
  report.printf("sensor_i2c_sda=%d\nsensor_i2c_scl=%d\n", board.sensors.i2cSda, board.sensors.i2cScl);
  report.printf("sdmmc_clk=%d\nsdmmc_cmd=%d\nsdmmc_d0=%d\n", board.sdmmc.clk, board.sdmmc.cmd, board.sdmmc.d0);
  report.printf("usb_detection_supported=%s\n", board.usbDetect >= 0 ? "true" : "false");
}
