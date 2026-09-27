#pragma once
#include <cstring>

namespace singlepagecache {
// Be conservative even for hidden/unsupported media and namespace prefixes.
inline bool isMediaElement(const char* name) {
  const char* colon = std::strrchr(name, ':');
  if (colon) name = colon + 1;
  return std::strcmp(name, "img") == 0 || std::strcmp(name, "image") == 0 || std::strcmp(name, "svg") == 0 ||
         std::strcmp(name, "object") == 0 || std::strcmp(name, "embed") == 0 || std::strcmp(name, "video") == 0 ||
         std::strcmp(name, "audio") == 0 || std::strcmp(name, "picture") == 0;
}
inline bool eligible(int spines, int spine, int pages, bool freshTextOnly, bool pageHasImages) {
  return spines == 1 && spine == 0 && pages == 1 && freshTextOnly && !pageHasImages;
}
}  // namespace singlepagecache
