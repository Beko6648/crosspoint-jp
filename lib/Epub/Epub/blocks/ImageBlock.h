#pragma once
#include <HalStorage.h>

#include <memory>
#include <string>

#include "Block.h"

class GfxRenderer;

class ImageBlock final : public Block {
 public:
  ImageBlock(const std::string& imagePath, int16_t width, int16_t height);
  ~ImageBlock() override = default;

  const std::string& getImagePath() const { return imagePath; }
  int16_t getWidth() const { return width; }
  int16_t getHeight() const { return height; }

  bool imageExists() const;
  // Build a missing raster-image pixel cache without drawing into the current framebuffer.
  // The page position is used to preserve the rendered Bayer dither pattern.
  enum class CacheResult { AlreadyValid, Generated, Failed };
  // No font eviction or framebuffer loan by default. A non-null invalidation
  // pointer permits a JPEG fallback loan: the caller must redraw the whole UI
  // after this call if it becomes true, including when generation fails.
  CacheResult ensurePixelCache(GfxRenderer& renderer, int x, int y, bool releaseFontCaches = false,
                               bool* framebufferInvalidated = nullptr) const;
  bool pregeneratePixelCache(GfxRenderer& renderer, int x, int y, bool* framebufferInvalidated = nullptr) const;

  BlockType getType() override { return IMAGE_BLOCK; }
  bool isEmpty() override { return false; }

  void render(GfxRenderer& renderer, const int x, const int y);
  bool serialize(FsFile& file);
  static std::unique_ptr<ImageBlock> deserialize(FsFile& file);

 private:
  std::string imagePath;
  int16_t width;
  int16_t height;
};
