#pragma once

#include <BuildScratch.h>
#include <GfxRenderer.h>
#include <Logging.h>

#include "ImageToFramebufferDecoder.h"

// No owned strings or heap allocations: diagnostics must also work at OOM.
class ImageDecodeDiagnostics {
 public:
  ImageDecodeDiagnostics(const char* format, const RenderConfig& config, size_t decoderBytes,
                         const GfxRenderer& renderer)
      : format_(format), config_(config), decoderBytes_(decoderBytes) {
    LOG_DBG("IMEM", "%s start mode=%s free=%u maxAlloc=%u decoder=%u target=%dx%d cache=%s fb=%u scratch=%u", format_,
            config_.writeToFramebuffer ? "draw" : "cache", ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
            (unsigned)decoderBytes_, config_.maxWidth, config_.maxHeight, config_.cachePath.c_str(),
            (unsigned)renderer.getBufferSize(), (unsigned)buildscratch::available());
  }

  bool admit(size_t minimumFree) {
    if ((!config_.writeToFramebuffer && config_.cachePath.empty()) || ESP.getFreeHeap() < minimumFree ||
        ESP.getMaxAllocHeap() < decoderBytes_)
      return fail("gate");
    return true;
  }

  void scratch(int sourceWidth, int sourceHeight, size_t grayBytes, size_t bandBytes) const {
    LOG_DBG("IMEM", "%s source=%dx%d gray=%u band=%u free=%u maxAlloc=%u", format_, sourceWidth, sourceHeight,
            (unsigned)grayBytes, (unsigned)bandBytes, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }

  bool fail(const char* stage) const {
    LOG_INF("IMEM", "%s failed stage=%s mode=%s free=%u maxAlloc=%u decoder=%u cache=%s", format_, stage,
            config_.writeToFramebuffer ? "draw" : "cache", ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
            (unsigned)decoderBytes_, config_.cachePath.c_str());
    return false;
  }

 private:
  const char* format_;
  const RenderConfig& config_;
  size_t decoderBytes_;
};
