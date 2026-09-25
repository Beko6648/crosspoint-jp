#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

inline std::map<std::string, std::vector<uint8_t>> files;
inline int writesUntilFailure = -1;
inline bool closeFails = false;
inline bool openFails = false;
class HalFile {
 public:
  std::string path;
  bool opened = false;
  size_t offset = 0;
  explicit operator bool() const { return opened; }
  bool seek(size_t pos) { offset = pos; return opened; }
  size_t size() { return files[path].size(); }
  int read(void* dest, size_t count) {
    if (!opened || offset + count > size()) return 0;
    std::memcpy(dest, files[path].data() + offset, count);
    offset += count;
    return static_cast<int>(count);
  }
  bool isOpen() const { return opened; }
  bool close() {
    opened = false;
    return !closeFails;
  }
  size_t write(const void* data, size_t n) {
    if (!opened || writesUntilFailure == 0) return 0;
    if (writesUntilFailure > 0) --writesUntilFailure;
    const auto* p = static_cast<const uint8_t*>(data);
    files[path].insert(files[path].end(), p, p + n);
    return n;
  }
};
struct TestStorage {
  bool openFileForRead(const char*, const std::string& path, HalFile& f) {
    if (!files.count(path)) return false;
    f.path = path;
    f.opened = true;
    return true;
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& f) {
    if (openFails) return false;
    files[path].clear();
    f.path = path;
    f.opened = true;
    return true;
  }
  bool remove(const char* path) { return files.erase(path) != 0; }
};
inline TestStorage Storage;
using FsFile = HalFile;
