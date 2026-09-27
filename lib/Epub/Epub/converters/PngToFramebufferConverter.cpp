#include "PngToFramebufferConverter.h"

#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <PNGdec.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include "DecoderFileScope.h"
#include "DirectPixelWriter.h"
#include "DitherUtils.h"
#include "ImageDecodeDiagnostics.h"
#include "PixelCache.h"
#include "PngDecodeBudget.h"
#include "PngDrawRecovery.h"

namespace {

// Context struct passed through PNGdec callbacks to avoid global mutable state.
// The draw callback receives this via pDraw->pUser (set by png.decode()).
// The file I/O callbacks receive the FsFile* via pFile->fHandle (set by pngOpen()).
struct PngContext {
  GfxRenderer* renderer{nullptr};
  const RenderConfig* config{nullptr};
  int screenWidth{0};
  int screenHeight{0};

  // Scaling state
  float scale{1.f};
  int srcWidth{0};
  int srcHeight{0};
  int dstWidth{0};
  int dstHeight{0};
  int lastDstY{-1};  // Track last rendered destination Y to avoid duplicates

  PixelCache cache;
  bool caching{false};

  uint8_t* grayLineBuffer{nullptr};
  uint32_t lastYieldMs{0};
};

// File I/O callbacks use pFile->fHandle to access the FsFile*,
// avoiding the need for global file state.
void* pngOpenWithHandle(const char* filename, int32_t* size) {
  FsFile* f = new (std::nothrow) FsFile();
  if (!f) return nullptr;
  if (!Storage.openFileForRead("PNG", std::string(filename), *f)) {
    delete f;
    return nullptr;
  }
  *size = f->size();
  return f;
}

void pngCloseWithHandle(void* handle) {
  FsFile* f = reinterpret_cast<FsFile*>(handle);
  if (f) {
    f->close();
    delete f;
  }
}

int32_t pngReadWithHandle(PNGFILE* pFile, uint8_t* pBuf, int32_t len) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  if (!f) return 0;
  return f->read(pBuf, len);
}

int32_t pngSeekWithHandle(PNGFILE* pFile, int32_t pos) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  if (!f) return -1;
  return f->seek(pos);
}

// Preserve the existing total-heap admission floor. It is not an estimate of
// decoder size: PNGdec 1.1.6 with our scanline setting is 58,464 bytes on C3.
// The shared gate separately checks sizeof(PNG) against the largest free block;
// gray-line and band allocations are checked individually after open().
constexpr size_t MIN_FREE_HEAP_FOR_PNG = 60 * 1024;

// PNGdec keeps TWO scanlines in its internal ucPixels buffer (current + previous)
// and each scanline includes a leading filter byte.
// Required storage is therefore approximately: 2 * (pitch + 1) + alignment slack.
// If PNG_MAX_BUFFERED_PIXELS is smaller than this requirement for a given image,
// PNGdec can overrun its internal buffer before our draw callback executes.
int bytesPerPixelFromType(int pixelType) {
  switch (pixelType) {
    case PNG_PIXEL_TRUECOLOR:
      return 3;
    case PNG_PIXEL_GRAY_ALPHA:
      return 2;
    case PNG_PIXEL_TRUECOLOR_ALPHA:
      return 4;
    case PNG_PIXEL_GRAYSCALE:
    case PNG_PIXEL_INDEXED:
    default:
      return 1;
  }
}

int packedRowBytes(int srcWidth, int bitsPerSample) { return (srcWidth * bitsPerSample + 7) / 8; }

int requiredPngInternalBufferBytes(int srcWidth, int pixelType, int bitsPerSample) {
  // +1 filter byte per scanline, *2 for current+previous lines, +32 for alignment margin.
  int pitch = srcWidth * bytesPerPixelFromType(pixelType);
  if ((pixelType == PNG_PIXEL_GRAYSCALE || pixelType == PNG_PIXEL_INDEXED) && bitsPerSample < 8) {
    pitch = packedRowBytes(srcWidth, bitsPerSample);
  }
  return ((pitch + 1) * 2) + 32;
}

bool isSupportedBitDepth(int pixelType, int bitsPerSample) {
  if (bitsPerSample == 8) return true;
  if (bitsPerSample != 1 && bitsPerSample != 2 && bitsPerSample != 4) return false;
  return pixelType == PNG_PIXEL_GRAYSCALE || pixelType == PNG_PIXEL_INDEXED;
}

uint8_t readPackedSample(const uint8_t* pixels, int x, int bitsPerSample) {
  if (bitsPerSample == 8) return pixels[x];

  const int bitOffset = x * bitsPerSample;
  const int shift = 8 - bitsPerSample - (bitOffset & 7);
  const uint8_t mask = (1U << bitsPerSample) - 1;
  return (pixels[bitOffset >> 3] >> shift) & mask;
}

uint8_t expandSampleToByte(uint8_t sample, int bitsPerSample) {
  if (bitsPerSample == 8) return sample;
  const uint8_t maxSample = (1U << bitsPerSample) - 1;
  return static_cast<uint8_t>((sample * 255U) / maxSample);
}

// Convert entire source line to grayscale with alpha blending to white background.
// Low-bit-depth grayscale/indexed scanlines are packed most-significant sample first.
// For indexed PNGs with tRNS chunk, alpha values are stored at palette[768] onwards.
// Processing the whole line at once improves cache locality and reduces per-pixel overhead.
void convertLineToGray(const uint8_t* pPixels, uint8_t* grayLine, int width, int pixelType, int bitsPerSample,
                       uint8_t* palette, int hasAlpha) {
  switch (pixelType) {
    case PNG_PIXEL_GRAYSCALE:
      if (bitsPerSample == 8) {
        memcpy(grayLine, pPixels, width);
      } else {
        for (int x = 0; x < width; x++) {
          grayLine[x] = expandSampleToByte(readPackedSample(pPixels, x, bitsPerSample), bitsPerSample);
        }
      }
      break;

    case PNG_PIXEL_TRUECOLOR:
      for (int x = 0; x < width; x++) {
        const uint8_t* p = &pPixels[x * 3];
        grayLine[x] = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
      }
      break;

    case PNG_PIXEL_INDEXED:
      if (palette) {
        if (hasAlpha) {
          for (int x = 0; x < width; x++) {
            const uint8_t idx = readPackedSample(pPixels, x, bitsPerSample);
            const uint8_t* p = &palette[idx * 3];
            uint8_t gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
            uint8_t alpha = palette[768 + idx];
            grayLine[x] = (uint8_t)((gray * alpha + 255 * (255 - alpha)) / 255);
          }
        } else {
          for (int x = 0; x < width; x++) {
            const uint8_t idx = readPackedSample(pPixels, x, bitsPerSample);
            const uint8_t* p = &palette[idx * 3];
            grayLine[x] = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
          }
        }
      } else {
        for (int x = 0; x < width; x++) {
          grayLine[x] = expandSampleToByte(readPackedSample(pPixels, x, bitsPerSample), bitsPerSample);
        }
      }
      break;

    case PNG_PIXEL_GRAY_ALPHA:
      for (int x = 0; x < width; x++) {
        uint8_t gray = pPixels[x * 2];
        uint8_t alpha = pPixels[x * 2 + 1];
        grayLine[x] = (uint8_t)((gray * alpha + 255 * (255 - alpha)) / 255);
      }
      break;

    case PNG_PIXEL_TRUECOLOR_ALPHA:
      for (int x = 0; x < width; x++) {
        const uint8_t* p = &pPixels[x * 4];
        uint8_t gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
        uint8_t alpha = p[3];
        grayLine[x] = (uint8_t)((gray * alpha + 255 * (255 - alpha)) / 255);
      }
      break;

    default:
      memset(grayLine, 128, width);
      break;
  }
}

int pngDrawCallback(PNGDRAW* pDraw) {
  PngContext* ctx = reinterpret_cast<PngContext*>(pDraw->pUser);
  if (!ctx || !ctx->config || !ctx->renderer || !ctx->grayLineBuffer) return 0;

  if (ctx->config->cancellation && ctx->config->cancellation->poll()) return 0;
  ImageToFramebufferDecoder::yieldDuringDecode(ctx->lastYieldMs);

  int srcY = pDraw->y;
  int srcWidth = ctx->srcWidth;

  // Calculate destination Y with scaling
  int dstY = (int)(srcY * ctx->scale);

  // Skip if we already rendered this destination row (multiple source rows map to same dest)
  if (dstY == ctx->lastDstY) return 1;
  ctx->lastDstY = dstY;

  // Check bounds
  if (dstY >= ctx->dstHeight) return 1;

  int outY = ctx->config->y + dstY;
  if (outY >= ctx->screenHeight) return 1;

  // Convert entire source line to grayscale (improves cache locality)
  convertLineToGray(pDraw->pPixels, ctx->grayLineBuffer, srcWidth, pDraw->iPixelType, pDraw->iBpp, pDraw->pPalette,
                    pDraw->iHasAlpha);

  // Render scaled row using Bresenham-style integer stepping (no floating-point division)
  int dstWidth = ctx->dstWidth;
  int outXBase = ctx->config->x;
  int screenWidth = ctx->screenWidth;
  bool useDithering = ctx->config->useDithering;
  bool caching = ctx->caching;
  bool writeToFramebuffer = ctx->config->writeToFramebuffer;

  // Pre-compute orientation and render-mode state once per row
  DirectPixelWriter pw;
  pw.init(*ctx->renderer);
  pw.beginRow(outY);

  // The cache streams to disk one row at a time. Flushing rows below this one
  // (PNGdec delivers scanlines top to bottom) repositions the single-row band.
  // A flush failure stops caching for the rest of the decode so we never write
  // past the band buffer; finalize() then drops the partial file.
  DirectCacheWriter cw;
  if (caching) {
    if (!ctx->cache.advanceTo(dstY)) {
      caching = false;
      ctx->caching = false;
    } else {
      cw.init(ctx->cache.buffer, ctx->cache.bytesPerRow, ctx->cache.bandRows, ctx->cache.originX);
      cw.beginRow(outY, ctx->config->y + ctx->cache.bandStart);
    }
  }

  int srcX = 0;
  int error = 0;

  for (int dstX = 0; dstX < dstWidth; dstX++) {
    int outX = outXBase + dstX;
    if (outX < screenWidth) {
      uint8_t gray = ctx->grayLineBuffer[srcX];

      uint8_t ditheredGray;
      if (useDithering) {
        ditheredGray = applyBayerDither4Level(gray, outX, outY);
      } else {
        ditheredGray = gray / 85;
        if (ditheredGray > 3) ditheredGray = 3;
      }
      if (writeToFramebuffer) pw.writePixel(outX, ditheredGray);
      if (caching) cw.writePixel(outX, ditheredGray);
    }

    // Bresenham-style stepping: advance srcX based on ratio srcWidth/dstWidth
    error += srcWidth;
    while (error >= dstWidth) {
      error -= dstWidth;
      srcX++;
    }
  }

  return 1;
}

}  // namespace

bool PngToFramebufferConverter::getDimensionsStatic(const std::string& imagePath, ImageDimensions& out) {
  // Width and height are fixed fields in the PNG IHDR chunk. Reading them
  // directly avoids allocating the PNG decoder while section pages and
  // font metrics are already resident in the constrained ESP32-C3 heap.
  FsFile file;
  if (!Storage.openFileForRead("PNG", imagePath, file)) {
    LOG_ERR("PNG", "Failed to open PNG header: %s", imagePath.c_str());
    return false;
  }

  uint8_t header[24];
  const int bytesRead = file.read(header, sizeof(header));
  file.close();
  if (bytesRead != static_cast<int>(sizeof(header))) {
    LOG_ERR("PNG", "Truncated PNG header: %s", imagePath.c_str());
    return false;
  }

  constexpr uint8_t pngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  const bool validIhdrLength = header[8] == 0 && header[9] == 0 && header[10] == 0 && header[11] == 13;
  if (memcmp(header, pngSignature, sizeof(pngSignature)) != 0 || !validIhdrLength ||
      memcmp(header + 12, "IHDR", 4) != 0) {
    LOG_ERR("PNG", "Invalid PNG signature or IHDR: %s", imagePath.c_str());
    return false;
  }

  const uint32_t width = (static_cast<uint32_t>(header[16]) << 24) | (static_cast<uint32_t>(header[17]) << 16) |
                         (static_cast<uint32_t>(header[18]) << 8) | header[19];
  const uint32_t height = (static_cast<uint32_t>(header[20]) << 24) | (static_cast<uint32_t>(header[21]) << 16) |
                          (static_cast<uint32_t>(header[22]) << 8) | header[23];
  return validateAndStoreDimensions(width, height, out, "PNG header");
}

bool PngToFramebufferConverter::decodeToFramebuffer(const std::string& imagePath, GfxRenderer& renderer,
                                                    const RenderConfig& config) {
  ImageDecodeDiagnostics diagnostics("PNG", config, sizeof(PNG), renderer);
  const size_t reserve = config.writeToFramebuffer ? 0 : config.pngHeapReserveBytes;
  if (reserve) {
    // IHDR probing closes its file before the large decoder allocation.
    ImageDimensions dims;
    if (!getDimensionsStatic(imagePath, dims)) return false;
    const size_t band = PixelCache::requiredBytes(config.maxWidth, config.maxHeight, 1);
    const size_t freeBytes = ESP.getFreeHeap();
    const size_t largest = ESP.getMaxAllocHeap();
    if (!band || !pngbudget::admits(freeBytes, largest, sizeof(PNG), dims.width, band, reserve)) {
      LOG_DBG("IPF", "PNG deferred stage=budget free=%u maxAlloc=%u decoder=%u gray=%u band=%u reserve=%u overhead=%u",
              (unsigned)freeBytes, (unsigned)largest, (unsigned)sizeof(PNG), (unsigned)dims.width, (unsigned)band,
              (unsigned)reserve, (unsigned)pngbudget::OVERHEAD_BYTES);
      return false;
    }
  }
  const auto beforeRecovery = std::make_pair(ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  if (pngdrawrecovery::recover(config.writeToFramebuffer, MIN_FREE_HEAP_FOR_PNG, sizeof(PNG),
                               renderer.getFontCacheManager(),
                               [] { return std::make_pair(ESP.getFreeHeap(), ESP.getMaxAllocHeap()); })) {
    LOG_INF("IMEM", "PNG draw recovery vertical free=%u->%u max=%u->%u decoder=%u", beforeRecovery.first,
            ESP.getFreeHeap(), beforeRecovery.second, ESP.getMaxAllocHeap(), static_cast<unsigned>(sizeof(PNG)));
  }
  if (!diagnostics.admit(MIN_FREE_HEAP_FOR_PNG)) return false;

  std::unique_ptr<PNG> png(new (std::nothrow) PNG());
  if (!png) return diagnostics.fail("decoder-allocation");

  PngContext ctx;
  ctx.renderer = &renderer;
  ctx.config = &config;
  ctx.screenWidth = renderer.getScreenWidth();
  ctx.screenHeight = renderer.getScreenHeight();

  int rc = png->open(imagePath.c_str(), pngOpenWithHandle, pngCloseWithHandle, pngReadWithHandle, pngSeekWithHandle,
                     pngDrawCallback);
  DecoderFileScope<PNG> fileScope(*png);
  if (rc != PNG_SUCCESS) {
    LOG_ERR("PNG", "Failed to open PNG: %d", rc);
    return diagnostics.fail("open");
  }

  ImageDimensions sourceDimensions;
  if (!validateAndStoreDimensions(png->getWidth(), png->getHeight(), sourceDimensions, "PNG")) return false;

  // Calculate output dimensions
  ctx.srcWidth = sourceDimensions.width;
  ctx.srcHeight = sourceDimensions.height;

  if (config.useExactDimensions && config.maxWidth > 0 && config.maxHeight > 0) {
    // Fit image within the exact target bounds while preserving aspect ratio.
    // The single ctx.scale used for row mapping requires uniform X/Y scaling;
    // non-uniform scaling would cause diagonal distortion.
    float scaleX = (float)config.maxWidth / ctx.srcWidth;
    float scaleY = (float)config.maxHeight / ctx.srcHeight;
    ctx.scale = (scaleX < scaleY) ? scaleX : scaleY;
    ctx.dstWidth = (int)(ctx.srcWidth * ctx.scale + 0.5f);
    ctx.dstHeight = (int)(ctx.srcHeight * ctx.scale + 0.5f);
  } else {
    // Calculate scale factor to fit within maxWidth/maxHeight
    float scaleX = (float)config.maxWidth / ctx.srcWidth;
    float scaleY = (float)config.maxHeight / ctx.srcHeight;
    ctx.scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (ctx.scale > 1.0f) ctx.scale = 1.0f;  // Don't upscale

    ctx.dstWidth = (int)(ctx.srcWidth * ctx.scale);
    ctx.dstHeight = (int)(ctx.srcHeight * ctx.scale);
  }
  if (ctx.dstWidth <= 0 || ctx.dstHeight <= 0) return diagnostics.fail("dimensions");
  ctx.lastDstY = -1;  // Reset row tracking

  LOG_DBG("PNG", "PNG %dx%d -> %dx%d (scale %.2f), bpp: %d", ctx.srcWidth, ctx.srcHeight, ctx.dstWidth, ctx.dstHeight,
          ctx.scale, png->getBpp());

  const int pixelType = png->getPixelType();
  const int bitsPerSample = png->getBpp();
  const int requiredInternal = requiredPngInternalBufferBytes(ctx.srcWidth, pixelType, bitsPerSample);
  if (requiredInternal > PNG_MAX_BUFFERED_PIXELS) {
    LOG_ERR(
        "PNG",
        "PNG row buffer too small: need %d bytes for width=%d type=%d bpp=%d, configured PNG_MAX_BUFFERED_PIXELS=%d",
        requiredInternal, ctx.srcWidth, pixelType, bitsPerSample, PNG_MAX_BUFFERED_PIXELS);
    LOG_ERR("PNG", "Aborting decode to avoid PNGdec internal buffer overflow");
    return false;
  }

  if (!isSupportedBitDepth(pixelType, bitsPerSample)) {
    warnUnsupportedFeature(
        "bit depth (" + std::to_string(bitsPerSample) + "bpp) for pixel type " + std::to_string(pixelType), imagePath);
    return false;
  }

  // Low-bit-depth rows are expanded to one byte per source pixel before dithering.
  // Keep this separate from PNGdec's packed scanline buffer and reject images
  // whose expanded row would exceed the bounded scratch allocation.
  constexpr size_t MAX_GRAY_LINE_BUFFER_BYTES = PNG_MAX_BUFFERED_PIXELS / 2;
  const size_t grayBufSize = static_cast<size_t>(ctx.srcWidth);
  if (grayBufSize > MAX_GRAY_LINE_BUFFER_BYTES) {
    LOG_ERR("PNG", "Expanded gray row too wide: need %u bytes for width=%d, max=%u", static_cast<unsigned>(grayBufSize),
            ctx.srcWidth, static_cast<unsigned>(MAX_GRAY_LINE_BUFFER_BYTES));
    return false;
  }

  diagnostics.scratch(ctx.srcWidth, ctx.srcHeight, grayBufSize,
                      PixelCache::requiredBytes(ctx.dstWidth, ctx.dstHeight, 1));
  ctx.grayLineBuffer = static_cast<uint8_t*>(malloc(grayBufSize));
  if (!ctx.grayLineBuffer) {
    LOG_ERR("PNG", "Failed to allocate gray line buffer");
    return diagnostics.fail("line-buffer");
  }

  // Stream the pixel cache to disk. PNGdec delivers source scanlines top to
  // bottom and we emit at most one (downscaled) output row per callback, so the
  // band only needs a single row. Streaming keeps the working set tiny, so
  // unlike the old full-image buffer it neither competes with the large decoder
  // nor forces larger images to skip caching - which previously meant a full
  // re-decode on every one of an image page's ~14 render passes.
  ctx.caching = !config.cachePath.empty();
  if (ctx.caching) {
    if (!ctx.cache.begin(config.cachePath, ctx.dstWidth, ctx.dstHeight, config.x, config.y, 1)) {
      ctx.caching = false;
      diagnostics.fail("cache-begin");
      if (!config.writeToFramebuffer) {
        free(ctx.grayLineBuffer);
        return false;
      }
    }
  }

  // Recheck actual headroom after open/line/cache allocations. A rejected
  // attempt removes its partial file before returning to the reader.
  if (reserve && ESP.getFreeHeap() < reserve + pngbudget::OVERHEAD_BYTES) {
    LOG_DBG("IPF", "PNG deferred stage=allocated free=%u reserve=%u overhead=%u", ESP.getFreeHeap(), (unsigned)reserve,
            (unsigned)pngbudget::OVERHEAD_BYTES);
    ctx.cache.abort();
    free(ctx.grayLineBuffer);
    return false;
  }

  unsigned long decodeStart = millis();
  ctx.lastYieldMs = decodeStart;
  rc = png->decode(&ctx, 0);
  unsigned long decodeTime = millis() - decodeStart;

  free(ctx.grayLineBuffer);
  ctx.grayLineBuffer = nullptr;

  if (config.cancellation && config.cancellation->cancelled) {
    ctx.cache.abort();
    LOG_DBG("PNG", "Cache-only decode cancelled; partial cache discarded");
    return false;
  }

  if (rc != PNG_SUCCESS) {
    LOG_ERR("PNG", "Decode failed: %d", rc);
    if (ctx.caching) ctx.cache.abort();
    return diagnostics.fail("decode");
  }

  LOG_DBG("PNG", "PNG decoding complete - render time: %lu ms", decodeTime);

  // Finalize the streamed cache (caching may have been cleared on a flush error).
  bool cacheComplete = false;
  if (!config.cachePath.empty()) {
    const bool cacheStarted = ctx.cache.started();
    cacheComplete = ctx.cache.finalize();
    if (!cacheComplete && cacheStarted) diagnostics.fail("finalize");
  }
  return config.writeToFramebuffer || cacheComplete;
}

bool PngToFramebufferConverter::supportsFormat(const std::string& extension) {
  return FsHelpers::hasPngExtension(extension);
}
