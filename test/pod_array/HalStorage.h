#pragma once
#include <cstdint>
#include <string>

struct FsFile {
  std::string bytes;
  size_t calls = 0;
  bool shortWrite = false;
  size_t write(const uint8_t* data, size_t count) {
    ++calls;
    if (shortWrite && count) --count;
    bytes.append(reinterpret_cast<const char*>(data), count);
    return count;
  }
  int read(void*, size_t) { return 0; }
};
