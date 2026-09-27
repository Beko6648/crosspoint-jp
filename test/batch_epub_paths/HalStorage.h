#pragma once
#include <algorithm>
#include <cstring>
#include <string>

constexpr int O_WRONLY = 1, O_CREAT = 2, O_TRUNC = 4;
struct FakeDisk {
  std::string bytes;
  bool exists = false, failOpen = false, failClose = false, shortWrite = false, shortRead = false;
  bool failSeek = false;
} inline disk;

class HalFile {
 public:
  bool opened = false;
  size_t offset = 0;
  operator bool() const { return opened; }
  bool close() {
    opened = false;
    return !disk.failClose;
  }
  size_t write(const uint8_t* data, size_t length) {
    if (disk.shortWrite && length) --length;
    disk.bytes.append(reinterpret_cast<const char*>(data), length);
    return length;
  }
  int read(void* data, size_t length) {
    size_t count = std::min(length, disk.bytes.size() - offset);
    if (disk.shortRead && count) --count;
    std::memcpy(data, disk.bytes.data() + offset, count);
    offset += count;
    return static_cast<int>(count);
  }
  size_t size() { return disk.bytes.size(); }
  size_t position() { return offset; }
  bool seekSet(size_t pos) {
    offset = pos;
    return !disk.failSeek;
  }
};
struct FakeStorage {
  bool ensureDirectoryExists(const char*) { return true; }
  HalFile open(const char*, int) {
    if (disk.failOpen) return {};
    disk.bytes.clear();
    disk.exists = true;
    return {true, 0};
  }
  bool openFileForRead(const char*, const char*, HalFile& file) {
    file = {!disk.failOpen && disk.exists, 0};
    return static_cast<bool>(file);
  }
  bool remove(const char*) {
    disk.exists = false;
    disk.bytes.clear();
    return true;
  }
} inline Storage;
