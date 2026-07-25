#pragma once

#include <string>

namespace BookmarkUtil {
std::string getBookmarksDir();
std::string getBookmarkPath(const std::string& bookPath);
// Restores the last complete bookmark file if a power loss interrupted an
// atomic replacement between moving the old file aside and installing the new one.
void recoverBookmarkFile(const std::string& bookmarkPath);
}  // namespace BookmarkUtil
