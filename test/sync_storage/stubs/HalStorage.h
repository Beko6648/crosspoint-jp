#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct FakeStorage;
class HalFile {
 public:
  FakeStorage* storage = nullptr;
  std::shared_ptr<std::string> bytes;
  std::string path;
  size_t offset = 0;
  size_t size() const { return bytes ? bytes->size() : 0; }
  size_t position() const { return offset; }
  int read(void* buffer, size_t length);
  int read() {
    uint8_t byte = 0;
    return read(&byte, 1) == 1 ? byte : -1;
  }
  size_t write(const uint8_t* buffer, size_t length);
  size_t write(uint8_t byte) { return write(&byte, 1); }
  bool close();
  void flush() {}
  explicit operator bool() const { return static_cast<bool>(bytes); }
};
using FsFile = HalFile;
class HalStorage {
 public:
  class StorageLock {
   public:
    StorageLock() {}
    ~StorageLock() {}
  };
};
struct FakeStorage {
  using Snapshot = std::map<std::string, std::string>;
  std::map<std::string, std::shared_ptr<std::string>> files;
  bool online = true;
  int failAt = 0, calls = 0;
  std::string failRead, failWrite, failClose, failRenameSource;
  unsigned long nowMs = 0;
  bool recording = false;
  std::vector<Snapshot> snapshots;
  bool fail() { return ++calls == failAt; }
  bool ready() const { return online; }
  bool ensureDirectoryExists(const char*) { return online; }
  bool mkdir(const char*) { return online; }
  bool exists(const char* p) const { return files.count(p) != 0; }
  Snapshot snapshot() const {
    Snapshot result;
    for (const auto& item : files) result[item.first] = *item.second;
    return result;
  }
  void record() {
    if (recording) snapshots.push_back(snapshot());
  }
  void restore(const Snapshot& value) {
    files.clear();
    for (const auto& item : value) files[item.first] = std::make_shared<std::string>(item.second);
  }
  bool remove(const char* p) {
    if (fail()) return false;
    const bool found = files.erase(p) != 0;
    record();
    return found;
  }
  bool rename(const char* a, const char* b) {
    if (fail() || a == failRenameSource || !exists(a) || exists(b)) return false;
    files[b] = files[a];
    files.erase(a);
    record();
    return true;
  }
  bool openFileForRead(const char*, const std::string& p, HalFile& f) {
    if (fail() || !exists(p.c_str())) return false;
    f = {this, files[p], p, 0};
    return true;
  }
  bool openFileForWrite(const char*, const std::string& p, HalFile& f) {
    if (fail()) return false;
    files[p] = std::make_shared<std::string>();
    f = {this, files[p], p, 0};
    record();
    return true;
  }
  void put(const std::string& p, const std::string& value) { files[p] = std::make_shared<std::string>(value); }
};
inline FakeStorage Storage;
inline unsigned long millis() { return Storage.nowMs; }
inline int HalFile::read(void* buffer, size_t length) {
  if (!bytes || storage->fail() || path == storage->failRead) return -1;
  const size_t count = std::min(length, bytes->size() - offset);
  if (count) std::memcpy(buffer, bytes->data() + offset, count);
  offset += count;
  return static_cast<int>(count);
}
inline size_t HalFile::write(const uint8_t* buffer, size_t length) {
  if (storage->fail() || path == storage->failWrite) {
    if (length) --length;
  }
  if (length) bytes->append(reinterpret_cast<const char*>(buffer), length);
  offset += length;
  storage->record();
  return length;
}
inline bool HalFile::close() { return !storage->fail() && path != storage->failClose; }
