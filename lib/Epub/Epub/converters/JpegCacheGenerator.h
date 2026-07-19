#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace JpegCacheGenerator {

struct Result {
  int sourceCount = 0;
  int validCacheCount = 0;
  int generatedCacheCount = 0;
  int invalidCacheCount = 0;
  int failedCacheCount = 0;
  bool scanComplete = false;
};

Result generateFromExtractedImages(const std::string& cacheRoot, const std::vector<bool>& eligibleSections,
                                   uint16_t viewportWidth, uint16_t viewportHeight, const char* logOrigin,
                                   const char* storageTag);

}  // namespace JpegCacheGenerator
