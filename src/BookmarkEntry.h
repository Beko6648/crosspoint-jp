#pragma once

#include <cstdint>
#include <string>

// A bookmark is deliberately page based.  The saved page location provides a
// fast match on every render; percentage is the fallback after repagination.
struct BookmarkEntry {
  std::string summary;
  float percentage = 0.0f;
  uint16_t spineIndex = 0;
  uint16_t chapterPageCount = 0;
  uint16_t chapterPage = 0;
};
