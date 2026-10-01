#include "BookmarkUtil.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

#include "sync/StorageIo.h"

namespace BookmarkUtil {

std::string getBookmarksDir() { return "/.crosspoint/bookmarks/"; }

std::string getBookmarkPath(const std::string& bookPath) {
  std::string name = bookPath;
  if (!name.empty() && name.front() == '/') name.erase(0, 1);
  std::replace(name.begin(), name.end(), '/', '_');
  std::replace(name.begin(), name.end(), '\\', '_');
  const size_t lastDot = name.find_last_of('.');
  if (lastDot != std::string::npos) name.erase(lastDot);
  return getBookmarksDir() + name + ".json";
}

void recoverBookmarkFile(const std::string& bookmarkPath) {
  if (!yomuka::sync::recoverFile(bookmarkPath)) {
    LOG_ERR("BKM", "Failed to restore bookmark backup: %s", bookmarkPath.c_str());
  }
}

}  // namespace BookmarkUtil
