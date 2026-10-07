#pragma once

#include <HalStorage.h>
#include <Logging.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "sync/ProgressStorage.h"

namespace ProgressFile {

// Verified single-file replacement retains a recoverable .bak. EPUB's
// canonical progress path also records content-bound timestamps without
// changing the legacy 4/6/7/8-byte progress payload.
inline bool writeAtomicPath(const std::string& finalPath, const uint8_t* data, size_t len,
                            bool recordLocalChange = true) {
  if (finalPath.find("/.crosspoint/books/") == 0 && finalPath.ends_with("/progress.bin")) {
    return yomuka::sync::saveProgress(finalPath, data, len, recordLocalChange);
  }
  return yomuka::sync::writeBytes(finalPath, data, len);
}

// Legacy TXT/XTC callers still store progress in their path-keyed cache
// directories. EPUB passes its fingerprint-keyed canonical file path above.
inline bool writeAtomic(const std::string& cachePath, const uint8_t* data, size_t len) {
  return writeAtomicPath(cachePath + "/progress.bin", data, len);
}

// EPUB progress historically used 4, 6, and 7 byte payloads. Keep all three
// readable during the fingerprint-path migration; callers use the first six
// bytes for the resume position and retain byte seven as the finished marker.
inline size_t readLegacyCompatible(const std::string& path, uint8_t (&data)[7]) {
  if (!yomuka::sync::recoverFile(path)) return 0;
  FsFile file;
  if (!Storage.openFileForRead("PRG", path, file)) return 0;
  const int bytesRead = file.read(data, sizeof(data));
  file.close();
  return bytesRead == 4 || bytesRead == 6 || bytesRead == 7 ? static_cast<size_t>(bytesRead) : 0;
}

}  // namespace ProgressFile
