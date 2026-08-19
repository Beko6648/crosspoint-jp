#pragma once

#include <functional>
#include <Epub.h>
#include <string>
#include <vector>

#include "../Activity.h"
#include "ReadingStatusHelper.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

class FileBrowserActivity final : public Activity {
 public:
  // Books is the normal reader browser. PickFirmware filters the view to .bin
  // files and returns the selected SD-card path to its caller. PickDirectory
  // shows folders only (plus a special "move here" entry) and returns the
  // chosen destination folder to move a previously-selected entry into.
  enum class Mode { Books, PickFirmware, PickDirectory };

 private:
  enum class DirectoryLoadResult { Loaded, NotDirectory, OpenFailed };

  struct DirectoryCacheEntry {
    std::string path;
    std::vector<std::string> files;
    std::vector<ReadingStatus> statuses;
    std::vector<Epub::CacheGenerationStatus> cacheStatuses;
  };

  static constexpr size_t DIRECTORY_CACHE_SIZE = 4;

  // Deletion
  void clearFileMetadata(const std::string& fullPath);
  enum class MoveResult { Success, TargetExists, IntoSelf };
  // Move (rename) fullPath into destDir; guards against self/descendant moves
  // and same-name collisions. Returns the outcome.
  MoveResult moveEntry(const std::string& fullPath, const std::string& destDir);

  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  bool lockLongPressBack = false;
  bool lockNextConfirmRelease = false;
  Mode mode = Mode::Books;

  // Source entry picked for a move (set before launching PickDirectory).
  std::string moveSourcePath;

  // Files state
  std::string basepath = "/";
  std::string loadedPath;
  std::vector<std::string> files;
  std::vector<ReadingStatus> fileStatuses;
  std::vector<Epub::CacheGenerationStatus> fileCacheStatuses;
  std::vector<DirectoryCacheEntry> directoryCache;

  // Data loading
  DirectoryLoadResult loadFiles(bool forceReload = false);
  void cacheCurrentDirectory();
  bool restoreCachedDirectory();
  void invalidateDirectoryCache(const std::string& path);
  size_t findEntry(const std::string& name) const;

 public:
  explicit FileBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string initialPath = "/",
                               Mode mode = Mode::Books)
      : Activity("FileBrowser", renderer, mappedInput), mode(mode),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
