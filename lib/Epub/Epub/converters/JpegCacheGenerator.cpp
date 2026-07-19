#include "JpegCacheGenerator.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>

#include <string_view>

#include "ImageCacheValidation.h"

namespace JpegCacheGenerator {
namespace {

bool parseExtractedJpegSection(const std::string_view fileName, const size_t sectionCount, size_t& sectionIndex) {
  constexpr std::string_view prefix = "img_";
  if (sectionCount == 0 || !FsHelpers::hasJpgExtension(fileName) ||
      fileName.size() <= prefix.size() || fileName.substr(0, prefix.size()) != prefix) {
    return false;
  }

  const size_t extensionStart = fileName.rfind('.');
  if (extensionStart == std::string_view::npos) return false;

  size_t cursor = prefix.size();
  size_t parsedSectionIndex = 0;
  const size_t sectionStart = cursor;
  while (cursor < extensionStart && fileName[cursor] >= '0' && fileName[cursor] <= '9') {
    const size_t digit = fileName[cursor] - '0';
    if (parsedSectionIndex > (sectionCount - 1) / 10) return false;
    parsedSectionIndex = parsedSectionIndex * 10 + digit;
    if (parsedSectionIndex >= sectionCount) return false;
    ++cursor;
  }
  if (cursor == sectionStart || cursor >= extensionStart || fileName[cursor] != '_') return false;

  ++cursor;
  const size_t imageIndexStart = cursor;
  while (cursor < extensionStart && fileName[cursor] >= '0' && fileName[cursor] <= '9') ++cursor;
  if (cursor == imageIndexStart || cursor != extensionStart) return false;

  sectionIndex = parsedSectionIndex;
  return true;
}

}  // namespace

Result generateFromExtractedImages(const std::string& cacheRoot, const std::vector<bool>& eligibleSections,
                                   const uint16_t viewportWidth, const uint16_t viewportHeight,
                                   const char* logOrigin, const char* storageTag) {
  Result result;
  auto dir = Storage.open(cacheRoot.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    LOG_ERR(logOrigin, "Failed to scan JPEG cache directory: %s", cacheRoot.c_str());
    return result;
  }

  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) {
      file.close();
      continue;
    }

    if (file.getName(name, sizeof(name)) == 0) {
      file.close();
      LOG_ERR(logOrigin, "Failed to read JPEG cache directory entry");
      dir.close();
      return result;
    }
    file.close();

    const std::string_view fileName(name);
    if (!FsHelpers::hasJpgExtension(fileName)) continue;

    size_t sectionIndex = 0;
    if (!parseExtractedJpegSection(fileName, eligibleSections.size(), sectionIndex) ||
        !eligibleSections[sectionIndex]) {
      continue;
    }

    result.sourceCount++;
    const std::string jpegPath = cacheRoot + "/" + name;
    const size_t dotPos = jpegPath.rfind('.');
    const std::string bmpCachePath = jpegPath.substr(0, dotPos) + ".pxc5.bmp";
    if (Storage.exists(bmpCachePath.c_str())) {
      if (ImageCacheValidation::validateBmpCacheFile(bmpCachePath)) {
        result.validCacheCount++;
        continue;
      }
      result.invalidCacheCount++;
      LOG_DBG(logOrigin, "Removing invalid JPEG cache: %s", bmpCachePath.c_str());
      Storage.remove(bmpCachePath.c_str());
    }

    FsFile jpegFile, bmpFile;
    if (!Storage.openFileForRead(storageTag, jpegPath, jpegFile)) {
      result.failedCacheCount++;
      LOG_ERR(logOrigin, "Failed to open JPEG: %s", jpegPath.c_str());
      continue;
    }
    if (!Storage.openFileForWrite(storageTag, bmpCachePath, bmpFile)) {
      jpegFile.close();
      result.failedCacheCount++;
      LOG_ERR(logOrigin, "Failed to create JPEG cache: %s", bmpCachePath.c_str());
      continue;
    }

    const bool success =
        JpegToBmpConverter::jpegFileToBmpStreamWithSize(jpegFile, bmpFile, viewportWidth, viewportHeight);
    bmpFile.flush();
    const size_t bmpSize = bmpFile.size();
    jpegFile.close();
    bmpFile.close();

    if (success && ImageCacheValidation::validateBmpCacheFile(bmpCachePath)) {
      result.generatedCacheCount++;
    } else {
      Storage.remove(bmpCachePath.c_str());
      result.failedCacheCount++;
      LOG_ERR(logOrigin, "Removed failed JPEG cache: %s (%lu bytes)", bmpCachePath.c_str(),
              static_cast<unsigned long>(bmpSize));
    }
  }

  dir.close();
  result.scanComplete = true;
  return result;
}

}  // namespace JpegCacheGenerator
