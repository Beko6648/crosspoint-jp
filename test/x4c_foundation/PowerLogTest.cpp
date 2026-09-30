#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

struct {
  bool debugDisplay = true, rtcEnabled = true;
} SETTINGS;
namespace BoardConfig {
struct {
  int usbDetect = -1;
} ACTIVE;
}  // namespace BoardConfig
int usbReads = 0, statusReads = 0, adcReads = 0;
bool x3 = false, voltageKnown = true;
struct {
  bool deviceIsX3() const { return x3; }
  bool isUsbConnected() const {
    ++usbReads;
    return true;
  }
} gpio;
struct {
  int getBatteryPercentage() const { return 73; }
} powerManager;
unsigned long millis() { return 1234; }
class BatteryMonitor {
 public:
  struct Status {
    bool millivoltsKnown;
    uint16_t millivolts;
  };
  Status readStatus() const {
    ++statusReads;
    return {voltageKnown, 3800};
  }
  int readMillivolts() const { return 3700; }
  static int percentageFromMillivolts(int) { return 60; }
};
#if !FREEINK_DEVICE_X4CLASSIC
// Deliberately absent from the X4C build: touching the C3 ADC/I2C path fails to compile.
constexpr int BAT_GPIO0 = 0;
int analogRead(int) {
  ++adcReads;
  return 123;
}
struct {
  int index = 0;
  void beginTransmission(int) {}
  void write(int) {}
  int endTransmission(bool) { return 0; }
  int requestFrom(uint8_t, uint8_t) {
    index = 0;
    return 2;
  }
  int read() { return index++ == 0 ? 0x74 : 0x0e; }
} Wire;
#endif
constexpr int O_WRONLY = 1, O_CREAT = 2, O_APPEND = 4;
std::string saved;
struct File {
  explicit operator bool() const { return true; }
  void write(const char* text, size_t length) { saved.assign(text, length); }
  void close() {}
};
struct {
  File open(const char*, int) { return {}; }
} Storage;
#define LOG_INF(...) ((void)0)
#include "PowerLog.inc"

bool has(const char* value) { return saved.find(value) != std::string::npos; }
int main() {
  appendPowerLog("SLEEP");
#if FREEINK_DEVICE_X4CLASSIC
  assert(has("device=X4 Classic ui_pct=73 voltage_pct=-1 raw_adc=-1 voltage_mV=3800 current_mA=-32769 usb=-1"));
  assert(statusReads == 1 && adcReads == 0 && usbReads == 0);
  voltageKnown = false;
  appendPowerLog("WAKE", "other");
  assert(has("voltage_mV=-1") && has("wake=other"));
#else
  assert(has("device=X4 ui_pct=73 voltage_pct=60 raw_adc=123 voltage_mV=3700 current_mA=-32769 usb=1"));
  assert(statusReads == 0 && adcReads == 1);
  x3 = true;
  appendPowerLog("WAKE", "power_button");
  assert(has("device=X3 ui_pct=73 voltage_pct=-1 raw_adc=-1 voltage_mV=3700 current_mA=3700 usb=1"));
  assert(adcReads == 1 && statusReads == 0);
#endif
  SETTINGS.debugDisplay = false;
  saved.clear();
  appendPowerLog("SLEEP");
  assert(saved.empty());
  puts("PASS: production power log backend, unavailable values, USB semantics and debug-off silence");
}
