#include "TlsHeapReclaim.h"

#include <Arduino.h>
#include <FontCacheManager.h>
#include <FontManager.h>
#include <GfxRenderer.h>
#include <Logging.h>

void reclaimHeapForTls(GfxRenderer& renderer, const char* tag) {
  const uint32_t heapBefore = ESP.getFreeHeap();
  const uint32_t blockBefore = ESP.getMaxAllocHeap();

  FontManager& fontManager = FontManager::getInstance();
  if (ExternalFont* uiFont = fontManager.getActiveUiFont()) uiFont->unload();
  if (ExternalFont* readerFont = fontManager.getActiveFont()) readerFont->unload();

  if (FontCacheManager* cacheManager = renderer.getFontCacheManager()) {
    cacheManager->clearCache();
    cacheManager->freeKernLigatureData();
  }

  LOG_DBG(tag, "Reclaimed for TLS: heap=%u->%u maxAlloc=%u->%u", heapBefore, ESP.getFreeHeap(), blockBefore,
          ESP.getMaxAllocHeap());
}
