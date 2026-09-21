#pragma once

#include <functional>
#include <string>

class OtaUpdater {
  bool updateAvailable = false;
  std::string latestVersion;
  std::string otaUrl;
  size_t otaSize = 0;
  size_t processedSize = 0;
  size_t totalSize = 0;
  bool render = false;

 public:
  enum OtaUpdaterError {
    OK = 0,
    NO_UPDATE,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    UPDATE_OLDER_ERROR,
    INTERNAL_UPDATE_ERROR,
    OOM_ERROR,
  };

  size_t getOtaSize() const { return otaSize; }

  size_t getProcessedSize() const { return processedSize; }

  size_t getTotalSize() const { return totalSize; }

  bool getRender() const { return render; }

  OtaUpdater() = default;
#ifdef SIMULATOR
  using ProgressCallback = void (*)(void*);
  bool isUpdateNewer() const { return updateAvailable; }
#else
  bool isUpdateNewer() const;
#endif
  const std::string& getLatestVersion() const;
  OtaUpdaterError checkForUpdate();
#ifdef SIMULATOR
  OtaUpdaterError installUpdate(ProgressCallback onProgress = nullptr, void* ctx = nullptr);
#else
  OtaUpdaterError installUpdate();
#endif
};
