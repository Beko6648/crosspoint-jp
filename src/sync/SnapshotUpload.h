#pragma once
#include <vector>

#include "SnapshotValidation.h"
namespace yomuka::sync {
// One multipart file, bounded in RAM. Nothing is published before the POST completes.
class SnapshotUpload {
  std::vector<uint8_t> bytes;
  unsigned files = 0;
  bool failed = false, ended = false;

 public:
  void clear() {
    std::vector<uint8_t>().swap(bytes);
    files = 0;
    failed = false;
    ended = false;
  }
  void start() {
    if (files == 0) {
      failed = false;
      ended = false;
    }
    ++files;
    if (files != 1) {
      failed = true;
      std::vector<uint8_t>().swap(bytes);
    }
  }
  size_t size() const { return bytes.size(); }
  bool append(const uint8_t* data, size_t length, bool memoryAvailable = true) {
    if (failed || ended || files != 1 || !memoryAvailable || length > 65536 - bytes.size() || (!data && length)) {
      failed = true;
      std::vector<uint8_t>().swap(bytes);
      return false;
    }
    if (length) {
      // Reserve explicitly to avoid an unchecked doubling allocation on ESP32.
      bytes.reserve(bytes.size() + length);
      bytes.insert(bytes.end(), data, data + length);
    }
    return true;
  }
  void finish() { ended = true; }
  void abort() {
    clear();
    failed = true;
  }
  bool parse(JsonDocument& output) {
    if (failed || files != 1 || !ended || bytes.empty()) return false;
    JsonDocument candidate;
    if (parseSnapshot(bytes.data(), bytes.size(), candidate) != SnapshotError::None) return false;
    output = std::move(candidate);
    std::vector<uint8_t>().swap(bytes);
    return true;
  }
};
}  // namespace yomuka::sync
