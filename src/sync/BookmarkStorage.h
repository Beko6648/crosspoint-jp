#pragma once

#include <vector>

#include "BookmarkEntry.h"
#include "StorageIo.h"

namespace yomuka {
namespace sync {
ReadStatus readBookmarks(const std::string& path, std::vector<BookmarkEntry>& value, uint32_t& updatedAt);
bool saveBookmarks(const std::string& path, const std::vector<BookmarkEntry>& value, bool recordLocalChange = true);
}  // namespace sync
}  // namespace yomuka
