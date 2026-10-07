#pragma once
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

#include "SnapshotValidation.h"
namespace yomuka::sync {
// One multipart file, bounded in RAM. Nothing is published before the POST completes.
class SnapshotUpload {
  std::unique_ptr<uint8_t[]> bytes;
  size_t length = 0;
  unsigned files = 0;
  bool failed = false, ended = false;
  const char* failure = nullptr;
  SnapshotError validation = SnapshotError::None;

 public:
  void clear() {
    bytes.reset();
    length = 0;
    files = 0;
    failure = nullptr;
    validation = SnapshotError::None;
    failed = false;
    ended = false;
  }
  void start() {
    if (files == 0) {
      failure = nullptr;
      validation = SnapshotError::None;
      failed = false;
      ended = false;
    }
    ++files;
    if (files != 1) {
      failure = "multiple-files";
      failed = true;
      bytes.reset();
      length = 0;
    }
  }
  size_t size() const { return length; }
  // Small snapshots need room for slots, decoded keys, and temporary scanner data,
  // not a fixed 32 KiB reader buffer. Retain the previous budget for larger files.
  // This is a conservative admission estimate, not a promise that allocation succeeds.
  static size_t requiredFree(size_t size) { return std::min(size * 16 + 8192, size * 3 + 32768); }
  static bool memoryAvailable(size_t size, size_t free, size_t largest) {
    return size <= 65536 && free >= requiredFree(size) && largest >= size + 4096;
  }
  const char* failureReason() const { return failure ? failure : "incomplete-or-empty"; }
  SnapshotError validationError() const { return validation; }
  bool append(const uint8_t* data, size_t count, bool memoryAvailable = true) {
    if (failed || ended || files != 1 || !memoryAvailable || count > 65536 - length || (!data && count)) {
      if (!failure) failure = count > 65536 - length ? "too-large" : !memoryAvailable ? "memory" : "invalid-state";
      failed = true;
      bytes.reset();
      length = 0;
      return false;
    }
    if (count) {
      std::unique_ptr<uint8_t[]> next(new (std::nothrow) uint8_t[length + count]);
      if (!next) {
        failed = true;
        failure = "allocation";
        bytes.reset();
        length = 0;
        return false;
      }
      if (length) std::memcpy(next.get(), bytes.get(), length);
      std::memcpy(next.get() + length, data, count);
      bytes = std::move(next);
      length += count;
    }
    return true;
  }
  void finish() { ended = true; }
  void abort() {
    clear();
    failed = true;
    failure = "aborted";
  }
  bool parse(JsonDocument& output, bool memoryAvailable = true) {
    if (failed || files != 1 || !ended || length == 0) return false;
    if (!memoryAvailable) {
      failure = "memory";
      return false;
    }
    JsonDocument candidate;
    validation = parseSnapshot(bytes.get(), length, candidate);
    if (validation != SnapshotError::None) {
      failure = "validation";
      return false;
    }
    output = std::move(candidate);
    bytes.reset();
    length = 0;
    return true;
  }
};
}  // namespace yomuka::sync
