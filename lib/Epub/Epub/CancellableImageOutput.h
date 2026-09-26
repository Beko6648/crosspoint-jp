#pragma once

#include <Print.h>

#include <functional>

// A latched cancellation remains distinguishable from a storage failure.
class CancellableImageOutput final : public Print {
 public:
  CancellableImageOutput(Print& output, const std::function<bool()>& cancel) : output(output), cancel(cancel) {}
  bool isCancelled() {
    if (!cancelled && cancel && cancel()) cancelled = true;
    return cancelled;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* data, size_t count) override { return isCancelled() ? 0 : output.write(data, count); }

 private:
  Print& output;
  const std::function<bool()>& cancel;
  bool cancelled = false;
};
