#pragma once

#include <Arduino.h>
#include <Imu.h>

namespace CrossPointOrientation {
enum Value : uint8_t { PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3 };
}

namespace CrossPointTiltPageTurn {
enum Value : uint8_t { TILT_OFF = 0, TILT_NORMAL = 1, TILT_INVERTED = 2 };
}

class HalTiltSensor;
extern HalTiltSensor halTiltSensor;

class HalTiltSensor {
 public:
  struct Diagnostics {
    bool available = false;
    bool awake = false;
    bool hasSample = false;
    uint8_t i2cAddress = 0;
    Imu::Sample sample{};
    float currentAxisDps = 0;
    float displayAxisDps = 0;
    char axisName[3] = "GX";
    float triggerThresholdDps = 270;
    float neutralThresholdDps = 50;
    float rightPeakDps = 0;
    float leftPeakDps = 0;
    float idleNoiseDps = 0;
    uint32_t readErrorCount = 0;
    uint16_t rightCrossings = 0;
    uint16_t leftCrossings = 0;
  };

  void begin();
  bool wake();
  bool deepSleep();
  bool isAvailable() const { return _available; }
  bool isAwake() const { return _isAwake; }
  void update(uint8_t mode, uint8_t orientation, bool inReader);
  bool wasTiltedForward();
  bool wasTiltedBack();
  bool hadActivity();
  void clearPendingEvents();

  bool beginDiagnostics(uint8_t orientation);
  void updateDiagnostics(uint8_t orientation);
  void endDiagnostics();
  void resetDiagnostics();
  Diagnostics getDiagnostics(uint8_t orientation) const;
  static constexpr float triggerThreshold() { return RATE_THRESHOLD_DPS; }
  static constexpr float neutralThreshold() { return NEUTRAL_RATE_DPS; }

 private:
  Imu _imu;
  bool _available = false;
  bool _isAwake = false;
  bool _diagnosticsActive = false;
  uint8_t _i2cAddr = 0;
  bool _tiltForwardEvent = false;
  bool _tiltBackEvent = false;
  bool _hadActivity = false;
  bool _inTilt = false;
  unsigned long _initMs = 0;
  unsigned long _lastTiltMs = 0;
  unsigned long _wakeMs = 0;
  unsigned long _lastPollMs = 0;
  Imu::Sample _sample{};
  bool _hasSample = false;
  uint32_t _readErrorCount = 0;
  float _rightPeakDps = 0;
  float _leftPeakDps = 0;
  float _idleNoiseDps = 0;
  float _diagnosticDisplayAxisDps = 0;
  uint16_t _rightCrossings = 0;
  uint16_t _leftCrossings = 0;
  unsigned long _diagnosticStartMs = 0;
  bool _diagnosticInMotion = false;
  unsigned long _diagnosticQuietSinceMs = 0;

  static constexpr float RATE_THRESHOLD_DPS = 270.0f;
  static constexpr float NEUTRAL_RATE_DPS = 50.0f;
  static constexpr unsigned long COOLDOWN_MS = 600;
  static constexpr unsigned long POLL_INTERVAL_MS = 50;
  static constexpr unsigned long WAKE_STABILIZE_MS = 300;
  static constexpr unsigned long SLEEP_STABILIZE_MS = 15;
  static constexpr unsigned long DIAGNOSTIC_REARM_MS = 250;

  bool readSample();
  float mappedAxis(uint8_t mode, uint8_t orientation, const Imu::Sample& sample) const;
  void recordDiagnostic(float axis, unsigned long now);
  uint8_t detectAddress() const;
};
