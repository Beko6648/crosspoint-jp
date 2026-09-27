#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>

// A per-run SD snapshot, not a persistent cache. Only the current path lives in
// RAM. Length prefixes preserve UTF-8, spaces and newline characters verbatim.
class BatchEpubPaths {
 public:
  ~BatchEpubPaths() {
    if (file) file.close();
    if (created) Storage.remove(path);
  }

  bool begin() {
    if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
    file = Storage.open(path, O_WRONLY | O_CREAT | O_TRUNC);
    created = static_cast<bool>(file);
    return created;
  }

  bool append(const std::string& value) {
    if (!file || reading || value.empty() || value.size() > UINT16_MAX) return false;
    const uint16_t size = static_cast<uint16_t>(value.size());
    const uint8_t length[] = {static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8)};
    if (file.write(length, sizeof(length)) != sizeof(length) ||
        file.write(reinterpret_cast<const uint8_t*>(value.data()), size) != size)
      return false;
    ++pathCount;
    storedBytes += sizeof(length) + size;
    return true;
  }

  // Closing detects write/sync errors before generation or status collection.
  bool rewind() {
    if (!file) return false;
    if (!reading) {
      if (!file.close()) return false;
      if (!Storage.openFileForRead("GENALL", path, file)) return false;
      reading = true;
    }
    readCount = 0;
    return file.size() == storedBytes && file.seekSet(0);
  }

  bool next(std::string& value) {
    if (!file || !reading || readCount >= pathCount) return false;
    uint8_t length[2];
    if (file.read(length, sizeof(length)) != sizeof(length)) return false;
    const size_t size = length[0] | (static_cast<size_t>(length[1]) << 8);
    const size_t position = file.position();
    if (!size || position > storedBytes || size > storedBytes - position) return false;
    value.resize(size);
    if (file.read(&value[0], size) != static_cast<int>(size)) return false;
    ++readCount;
    return true;
  }

  int count() const { return pathCount; }
  size_t bytes() const { return storedBytes; }

 private:
  static constexpr const char* path = "/.crosspoint/.batch-epub-paths.tmp";
  HalFile file;
  int pathCount = 0;
  int readCount = 0;
  size_t storedBytes = 0;
  bool reading = false;
  bool created = false;
};
