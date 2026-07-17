#include "ReadingStatusHelper.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {

bool getCacheEntryName(const std::string& filepath, std::string& entryName, bool& isEpub) {
  const char* prefix;
  if (FsHelpers::hasEpubExtension(filepath)) {
    prefix = "epub_";
    isEpub = true;
  } else if (FsHelpers::hasXtcExtension(filepath)) {
    prefix = "xtc_";
    isEpub = false;
  } else if (FsHelpers::hasTxtExtension(filepath) || FsHelpers::hasMarkdownExtension(filepath)) {
    prefix = "txt_";
    isEpub = false;
  } else {
    return false;
  }

  entryName = prefix + std::to_string(std::hash<std::string>{}(filepath));
  return true;
}

ReadingStatus readProgress(const std::string& progressPath, bool isEpub) {
  FsFile file;
  if (!Storage.openFileForRead("RSH", progressPath, file)) {
    return ReadingStatus::Unread;
  }

  uint8_t data[7];
  const int bytesRead = file.read(data, sizeof(data));
  file.close();

  if (bytesRead <= 0) {
    return ReadingStatus::Unread;
  }

  const int flagOffset = isEpub ? 6 : 4;
  return bytesRead > flagOffset && data[flagOffset] == 1 ? ReadingStatus::Finished : ReadingStatus::Reading;
}

}  // namespace

ReadingStatus getReadingStatus(const std::string& filepath, const std::string& cacheDir) {
  std::string cacheEntryName;
  bool isEpub;
  if (!getCacheEntryName(filepath, cacheEntryName, isEpub)) {
    return ReadingStatus::Unread;
  }

  return readProgress(cacheDir + "/" + cacheEntryName + "/progress.bin", isEpub);
}

void getReadingStatuses(const std::string& basePath, const std::vector<std::string>& filenames,
                        const std::string& cacheDir, std::vector<ReadingStatus>& statuses) {
  const unsigned long startedAt = millis();
  statuses.assign(filenames.size(), ReadingStatus::Unread);

  std::vector<std::string> cacheEntries;
  uint32_t scannedEntries = 0;
  bool cacheRootOpen = false;
  bool cacheRootIsDirectory = false;
  {
    auto cacheRoot = Storage.open(cacheDir.c_str());
    cacheRootOpen = static_cast<bool>(cacheRoot);
    if (cacheRootOpen && cacheRoot.isDirectory()) {
      cacheRootIsDirectory = true;
      char name[128];
      for (auto entry = cacheRoot.openNextFile(); entry; entry = cacheRoot.openNextFile()) {
        entry.getName(name, sizeof(name));
        cacheEntries.emplace_back(name);
        ++scannedEntries;
      }
    }
  }
  std::sort(cacheEntries.begin(), cacheEntries.end());

  std::string fullBase = basePath;
  if (fullBase.back() != '/') fullBase += '/';

  uint32_t books = 0;
  uint32_t progressReads = 0;
  for (size_t i = 0; i < filenames.size(); ++i) {
    std::string cacheEntryName;
    bool isEpub;
    const std::string filepath = fullBase + filenames[i];
    if (!getCacheEntryName(filepath, cacheEntryName, isEpub)) continue;

    ++books;
    if (!std::binary_search(cacheEntries.begin(), cacheEntries.end(), cacheEntryName)) continue;

    statuses[i] = readProgress(cacheDir + "/" + cacheEntryName + "/progress.bin", isEpub);
    ++progressReads;
  }

  LOG_DBG("RSH",
          "Status index: books=%lu cacheEntries=%lu progressReads=%lu openNext=%lu getName=%lu isDirectory=%lu "
          "close=%lu total=%lu ms",
          static_cast<unsigned long>(books), static_cast<unsigned long>(scannedEntries),
          static_cast<unsigned long>(progressReads),
          static_cast<unsigned long>(cacheRootIsDirectory ? scannedEntries + 1 : 0),
          static_cast<unsigned long>(scannedEntries), static_cast<unsigned long>(cacheRootOpen ? 1 : 0),
          static_cast<unsigned long>(cacheRootIsDirectory ? scannedEntries + 2 : 1), millis() - startedAt);
}

bool markAsFinished(const std::string& filepath, const std::string& cacheDir) {
  const char* prefix;
  bool isEpub;
  if (FsHelpers::hasEpubExtension(filepath)) {
    prefix = "epub_";
    isEpub = true;
  } else if (FsHelpers::hasXtcExtension(filepath)) {
    prefix = "xtc_";
    isEpub = false;
  } else if (FsHelpers::hasTxtExtension(filepath) || FsHelpers::hasMarkdownExtension(filepath)) {
    prefix = "txt_";
    isEpub = false;
  } else {
    return false;
  }

  const std::string hash = std::to_string(std::hash<std::string>{}(filepath));
  const std::string bookDir = cacheDir + "/" + prefix + hash;
  const std::string progressPath = bookDir + "/progress.bin";

  // EPUB=7, XTC/TXT=5
  const size_t recordSize = isEpub ? 7 : 5;
  const size_t flagOffset = isEpub ? 6 : 4;

  // 既存progress.binを読み込んで読書位置を保持する（なければゼロ初期化）
  uint8_t data[7] = {0};
  FsFile rf;
  if (Storage.openFileForRead("RSH", progressPath, rf)) {
    rf.read(data, recordSize);
    rf.close();
  }
  data[flagOffset] = 1;

  // ディレクトリを確保してから書き込む
  Storage.mkdir(cacheDir.c_str());
  Storage.mkdir(bookDir.c_str());

  FsFile wf;
  if (!Storage.openFileForWrite("RSH", progressPath, wf)) {
    LOG_ERR("RSH", "markAsFinished: Could not open %s for write", progressPath.c_str());
    return false;
  }
  wf.write(data, recordSize);
  wf.close();
  LOG_DBG("RSH", "Marked as finished: %s", filepath.c_str());
  return true;
}
