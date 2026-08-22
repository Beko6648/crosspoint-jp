#pragma once
#include <HalStorage.h>

#include <cstdint>
#include <memory>
#include <string>

class GfxRenderer;

struct ImageDimensions {
  int16_t width;
  int16_t height;
};

struct RenderConfig {
  int x, y;
  int maxWidth, maxHeight;
  bool useGrayscale = true;
  bool useDithering = true;
  bool performanceMode = false;
  bool useExactDimensions = false;  // If true, use maxWidth/maxHeight as exact output size (no recalculation)
  bool writeToFramebuffer = true;  // Cache-only generation keeps the progress UI intact
  std::string cachePath;            // If non-empty, decoder will write pixel cache to this path
};

class ImageToFramebufferDecoder {
 public:
  virtual ~ImageToFramebufferDecoder() = default;

  virtual bool decodeToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config) = 0;

  virtual bool getDimensions(const std::string& imagePath, ImageDimensions& dims) const = 0;

  virtual const char* getFormatName() const = 0;

  // Decoder callbacks call this periodically so multi-second image work does
  // not starve the idle task or watchdog.
  static void yieldDuringDecode(uint32_t& lastYieldMs);

  // Validate header dimensions before they are narrowed into ImageDimensions.
  // Shared by the header probes and full decoders.
  static bool validateAndStoreDimensions(int64_t width, int64_t height, ImageDimensions& out, const char* format);

 protected:
  // Size validation helpers
  // JPEG/PNG decoders use streaming (row-by-row) decode with built-in scaling,
  // so source pixel count does not determine memory usage. This limit is a safety
  // net against absurdly large images only.
  static constexpr int64_t MAX_SOURCE_DIMENSION = INT16_MAX;
  static constexpr int64_t MAX_SOURCE_PIXELS = 25000000;  // Preserve Yomuka's existing 25 MP ceiling.

  void warnUnsupportedFeature(const std::string& feature, const std::string& imagePath);
};
