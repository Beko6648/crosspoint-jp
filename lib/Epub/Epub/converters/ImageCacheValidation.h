#pragma once

#include <Bitmap.h>
#include <HalStorage.h>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace ImageCacheValidation {

struct PixelCacheInfo {
  uint16_t width = 0;
  uint16_t height = 0;
};

inline bool validatePixelCache(FsFile& file, const int expectedWidth, const int expectedHeight,
                               PixelCacheInfo* info = nullptr) {
  if (!file || !file.seek(0)) return false;

  PixelCacheInfo parsed;
  if (file.read(&parsed.width, sizeof(parsed.width)) != static_cast<int>(sizeof(parsed.width)) ||
      file.read(&parsed.height, sizeof(parsed.height)) != static_cast<int>(sizeof(parsed.height)) ||
      parsed.width == 0 || parsed.height == 0) {
    return false;
  }

  if ((expectedWidth > 0 && std::abs(static_cast<int>(parsed.width) - expectedWidth) > 1) ||
      (expectedHeight > 0 && std::abs(static_cast<int>(parsed.height) - expectedHeight) > 1)) {
    return false;
  }

  const uint64_t bytesPerRow = (static_cast<uint64_t>(parsed.width) + 3) / 4;
  const uint64_t expectedSize = sizeof(parsed.width) + sizeof(parsed.height) + bytesPerRow * parsed.height;
  if (expectedSize != file.size()) return false;

  if (info) *info = parsed;
  return true;
}

inline bool validatePixelCacheFile(const std::string& path, const int expectedWidth, const int expectedHeight) {
  FsFile file;
  if (!Storage.openFileForRead("IMG", path, file)) return false;
  const bool valid = validatePixelCache(file, expectedWidth, expectedHeight);
  file.close();
  return valid;
}

inline bool validateBmpCacheFile(const std::string& path) {
  FsFile file;
  if (!Storage.openFileForRead("IMG", path, file)) return false;
  Bitmap bitmap(file);
  const bool valid = bitmap.parseHeaders() == BmpReaderError::Ok;
  file.close();
  return valid;
}

}  // namespace ImageCacheValidation
