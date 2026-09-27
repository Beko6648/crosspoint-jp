#include "GenerateAllCacheActivity.h"

#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/Section.h>
#include <Epub/converters/ImageCacheValidation.h>
#include <Epub/converters/ImagePixelCachePath.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <vector>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReadingStatusHelper.h"
#include "SdCardFontGlobals.h"
#include "components/UITheme.h"
#include "components/UiLayout.h"
#include "fontIds.h"
#include "util/BatchEpubPaths.h"
#include "util/CacheGenerationControls.h"
#include "util/CacheProgressPolicy.h"

namespace {

constexpr int STATUS_BAR_CONTENT_GUARD = 8;

#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
void logBatchMemory(const char* stage, const int book = -1) {
  LOG_INF("BCMEM", "stage=%s book=%d free=%u maxAlloc=%u", stage, book, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

struct BatchBookMemoryProbe {
  int book;
  ~BatchBookMemoryProbe() { logBatchMemory("book-released", book); }
};
#endif

int getStatusBarContentReservation(const int statusBarHeight) {
  return statusBarHeight > 0 ? statusBarHeight + STATUS_BAR_CONTENT_GUARD : 0;
}

enum class PathScanResult { Complete, Cancelled, Failed };

// Recursively scan a directory for EPUB files
PathScanResult findEpubFiles(const char* dirPath, BatchEpubPaths& results, CacheGenerationControls& controls,
                             GfxRenderer& renderer) {
  if (controls.shouldCancel(renderer)) return PathScanResult::Cancelled;
  auto dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return PathScanResult::Failed;
  }

  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (controls.shouldCancel(renderer)) {
      file.close();
      dir.close();
      return PathScanResult::Cancelled;
    }
    file.getName(name, sizeof(name));
    if (name[0] == '.') {
      file.close();
      continue;
    }

    std::string fullPath = std::string(dirPath);
    if (fullPath.back() != '/') fullPath += '/';
    fullPath += name;

    if (file.isDirectory()) {
      file.close();
      const auto result = findEpubFiles(fullPath.c_str(), results, controls, renderer);
      if (result != PathScanResult::Complete) {
        dir.close();
        return result;
      }
    } else {
      if (FsHelpers::hasEpubExtension(std::string_view(name))) {
        if (!results.append(fullPath)) {
          file.close();
          dir.close();
          return PathScanResult::Failed;
        }
      }
      file.close();
    }
  }
  dir.close();
  return PathScanResult::Complete;
}

int pregeneratePixelCaches(const Page& page, GfxRenderer& renderer, const int xOffset, const int yOffset,
                           bool& framebufferInvalidated) {
  int generated = 0;
  for (const auto& element : page.elements) {
    if (element->getTag() != TAG_PageImage) continue;
    const auto& pageImage = static_cast<const PageImage&>(*element);
    const auto& image = pageImage.getImageBlock();
    if (image.pregeneratePixelCache(renderer, pageImage.xPos + xOffset, pageImage.yPos + yOffset,
                                    &framebufferInvalidated))
      generated++;
  }
  return generated;
}

int pregeneratePixelCachesFromCachedSection(Section& section, GfxRenderer& renderer, const int xOffset,
                                            const int yOffset, int& pagesScanned, bool& framebufferInvalidated) {
  int generated = 0;
  for (uint16_t pageIndex = 0; pageIndex < section.pageCount; ++pageIndex) {
    auto page = section.loadPageFromSectionFile(pageIndex);
    if (!page) continue;
    ++pagesScanned;
    if (page->hasImages())
      generated += pregeneratePixelCaches(*page, renderer, xOffset, yOffset, framebufferInvalidated);
  }
  return generated;
}

struct PixelCachePreflightResult {
  explicit PixelCachePreflightResult(const int spineCount) : sectionsNeedingPageScan(spineCount, false) {}

  std::vector<bool> sectionsNeedingPageScan;
  int sourceCount = 0;
  int validCacheCount = 0;
  int missingOrInvalidCacheCount = 0;
  int removedZeroLengthSourceCount = 0;
  bool complete = false;
};

bool isRasterImage(const std::string_view fileName) {
  return FsHelpers::hasPngExtension(fileName) || FsHelpers::hasJpgExtension(fileName);
}

bool parseRasterSourceSection(const std::string_view fileName, const int spineCount, int& sectionIndex) {
  constexpr std::string_view prefix = "img_";
  const size_t extensionStart = fileName.rfind('.');
  if (!isRasterImage(fileName) || extensionStart == std::string_view::npos || extensionStart <= prefix.size() ||
      fileName.substr(0, prefix.size()) != prefix) {
    return false;
  }

  size_t cursor = prefix.size();
  int parsedSectionIndex = 0;
  const size_t sectionStart = cursor;
  while (cursor < extensionStart && fileName[cursor] >= '0' && fileName[cursor] <= '9') {
    const int digit = fileName[cursor] - '0';
    if (parsedSectionIndex > (spineCount - 1) / 10) return false;
    parsedSectionIndex = parsedSectionIndex * 10 + digit;
    if (parsedSectionIndex >= spineCount) return false;
    ++cursor;
  }
  if (cursor == sectionStart || cursor >= extensionStart || fileName[cursor] != '_') return false;

  ++cursor;
  const size_t imageIndexStart = cursor;
  while (cursor < extensionStart && fileName[cursor] >= '0' && fileName[cursor] <= '9') ++cursor;
  if (cursor == imageIndexStart || cursor != extensionStart) return false;

  sectionIndex = parsedSectionIndex;
  return true;
}

PixelCachePreflightResult inspectPixelCaches(const std::string& cacheRoot, const int spineCount) {
  PixelCachePreflightResult result(spineCount);
  std::vector<std::string> zeroLengthSourcePaths;
  auto dir = Storage.open(cacheRoot.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return result;
  }

  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) {
      file.close();
      continue;
    }

    if (file.getName(name, sizeof(name)) == 0) {
      file.close();
      dir.close();
      return result;
    }
    const size_t sourceSize = file.size();
    file.close();

    const std::string_view fileName(name);
    if (!isRasterImage(fileName)) continue;
    if (fileName.size() < 4 || fileName.substr(0, 4) != "img_") continue;

    int sectionIndex = 0;
    if (!parseRasterSourceSection(fileName, spineCount, sectionIndex)) {
      // Unexpected raster-image names make it unsafe to assume the directory scan was complete.
      dir.close();
      return result;
    }

    const std::string sourcePath = cacheRoot + "/" + name;
    if (sourceSize == 0) {
      zeroLengthSourcePaths.push_back(sourcePath);
      continue;
    }

    result.sourceCount++;
    const std::string pixelCachePath = getImagePixelCachePath(sourcePath);
    if (Storage.exists(pixelCachePath.c_str()) && ImageCacheValidation::validatePixelCacheFile(pixelCachePath, 0, 0)) {
      result.validCacheCount++;
    } else {
      result.missingOrInvalidCacheCount++;
      result.sectionsNeedingPageScan[sectionIndex] = true;
    }
  }

  dir.close();
  for (const auto& sourcePath : zeroLengthSourcePaths) {
    if (!Storage.remove(sourcePath.c_str())) {
      LOG_ERR("GENALL", "Failed to remove zero-length extracted image: %s", sourcePath.c_str());
      return result;
    }
    result.removedZeroLengthSourceCount++;
    LOG_DBG("GENALL", "Removed zero-length extracted image: %s", sourcePath.c_str());
  }
  result.complete = true;
  return result;
}

}  // namespace

void GenerateAllCacheActivity::onEnter() {
  Activity::onEnter();
  state = CONFIRMING;
  requestUpdate();
}

void GenerateAllCacheActivity::onExit() {
  Activity::onExit();
  // Release the SD card font caches built during cache generation.  The loop
  // clears caches *before* each book (max heap for layout), but never after
  // the last book, so its advance tables + prewarm data (~130KB) stay resident
  // and starve the large contiguous page buffer XTC needs (~96KB/page) when
  // another book is opened afterwards -> "memory error".  Free them here.
  if (auto* fcm = renderer.getFontCacheManager()) {
    fcm->clearCache();
    fcm->freeKernLigatureData();
  }
}

bool GenerateAllCacheActivity::summarizeCacheStatuses(BatchEpubPaths& epubFiles) {
  if (!epubFiles.rewind()) return false;
  completeCount = 0;
  resumableCount = 0;
  notGeneratedCount = 0;
  std::string epubPath;
  for (int i = 0; i < epubFiles.count(); ++i) {
    if (!epubFiles.next(epubPath)) return false;
    switch (Epub(epubPath, "/.crosspoint").getCacheGenerationStatus()) {
      case Epub::CacheGenerationStatus::Complete:
        ++completeCount;
        break;
      case Epub::CacheGenerationStatus::Resumable:
        ++resumableCount;
        break;
      case Epub::CacheGenerationStatus::NotGenerated:
        ++notGeneratedCount;
        break;
    }
  }
  return true;
}

std::string GenerateAllCacheActivity::cacheGenerationResultText() const {
  return std::string(tr(STR_CACHE_SUMMARY_COMPLETE)) + ": " + std::to_string(completeCount) + "  " +
         tr(STR_CACHE_SUMMARY_RESUMABLE) + ": " + std::to_string(resumableCount) + "  " +
         tr(STR_CACHE_SUMMARY_NOT_GENERATED) + ": " + std::to_string(notGeneratedCount);
}

void GenerateAllCacheActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageHeight = renderer.getScreenHeight();
  const auto layout = UiLayout::from(renderer);
  const int centerOffset = layout.content.x + layout.content.width / 2 - renderer.getScreenWidth() / 2;
  const auto drawCentered = [this, centerOffset](const int y, const char* text, const bool clear = true,
                                                 const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
    renderer.drawCenteredTextOffset(UI_10_FONT_ID, y, text, clear, centerOffset, style);
  };

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{layout.content.x, metrics.topPadding, layout.content.width, metrics.headerHeight},
                 tr(STR_GENERATE_ALL_CACHE));

  if (state == CONFIRMING) {
    drawCentered(pageHeight / 2 - 20, tr(STR_GENERATE_CACHE));
    drawCentered(pageHeight / 2 + 10, tr(STR_GENERATE_CACHE_NOTE));

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == GENERATING) {
    drawCentered(pageHeight / 2, tr(STR_GENERATING_ALL_CACHE));
    drawCentered(pageHeight / 2 + 25, tr(STR_CACHE_CANCEL_HINT_LINE1));
    drawCentered(pageHeight / 2 + 45, tr(STR_CACHE_CANCEL_HINT_LINE2));
    const auto cancelLabels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, cancelLabels.btn1, cancelLabels.btn2, cancelLabels.btn3, cancelLabels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == SUCCESS) {
    drawCentered(pageHeight / 2 - 20, tr(STR_CACHE_GENERATED), true, EpdFontFamily::BOLD);
    std::string resultText = cacheGenerationResultText();
    drawCentered(pageHeight / 2 + 10, resultText.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == INTERRUPTED) {
    drawCentered(pageHeight / 2 - 20, tr(STR_CACHE_INTERRUPTED), true, EpdFontFamily::BOLD);
    std::string resultText = cacheGenerationResultText();
    drawCentered(pageHeight / 2 + 10, resultText.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == FAILED) {
    drawCentered(pageHeight / 2 - 20, tr(STR_SD_CARD_ERROR), true, EpdFontFamily::BOLD);
    drawCentered(pageHeight / 2 + 10, tr(STR_CACHE_INTERRUPTED));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

void GenerateAllCacheActivity::generateAllCaches() {
  // Invalidate before the first mutation, including interrupted/failed runs.
  // The browser rebuilds visible entries from authoritative per-book files.
  if (!invalidateBookListStatusIndex("/.crosspoint")) {
    LOG_ERR("GENALL", "Could not invalidate book-list status summary");
  }
  const uint32_t generationStartedAt = millis();
  LOG_DBG("GENALL", "Scanning for EPUB files...");

  const uint32_t scanStartedAt = millis();
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
  logBatchMemory("before-scan");
#endif
  BatchEpubPaths epubFiles;
  if (!epubFiles.begin()) {
    LOG_ERR("GENALL", "Could not create temporary EPUB list");
    state = FAILED;
    requestUpdate();
    return;
  }
  CacheGenerationControls controls(mappedInput);
  const auto scanResult = findEpubFiles("/", epubFiles, controls, renderer);
  LOG_DBG("GENALL", "EPUB scan completed in %lu ms", millis() - scanStartedAt);
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
  LOG_INF("BCMEM", "paths count=%d diskBytes=%u scanComplete=%d", epubFiles.count(),
          static_cast<unsigned>(epubFiles.bytes()), scanResult == PathScanResult::Complete);
  logBatchMemory("after-scan");
#endif
  totalCount = epubFiles.count();
  if (scanResult == PathScanResult::Failed || !epubFiles.rewind()) {
    LOG_ERR("GENALL", "Could not write/read temporary EPUB list");
    state = FAILED;
    requestUpdate();
    return;
  }
  if (scanResult == PathScanResult::Cancelled) {
    const bool summarized = summarizeCacheStatuses(epubFiles);
    state = summarized ? INTERRUPTED : FAILED;
    requestUpdate();
    return;
  }

  LOG_DBG("GENALL", "Found %d EPUB files", totalCount);

  if (totalCount == 0) {
    state = SUCCESS;
    requestUpdate();
    return;
  }

#if defined(CACHE_STORAGE_FAULT_INJECTION)
  struct CacheStorageFaultInjectionScope {
    CacheStorageFaultInjectionScope() {
      Section::setCacheStorageFaultInjectionActive(true);
      LOG_INF("SDFI", "event=armed point=temp_html_open spine=%d", CACHE_STORAGE_FAULT_SPINE);
    }
    ~CacheStorageFaultInjectionScope() { Section::setCacheStorageFaultInjectionActive(false); }
  } cacheStorageFaultInjectionScope;
#endif

  // Show progress popup
  const uint32_t initialDisplayStartedAt = millis();
  std::string progressDetail = std::string(tr(STR_CACHE_BOOK)) + " 0/" + std::to_string(totalCount);
  Rect popupRect = GUI.drawProgressPopup(renderer, tr(STR_GENERATING_ALL_CACHE), progressDetail.c_str());
  uint32_t progressDisplayMs = millis() - initialDisplayStartedAt;
  int lastDisplayedProgress = 0;
  CacheProgressPolicy progressPolicy(millis());
  bool framebufferInvalidated = false;
  const auto restoreProgress = [&] {
    if (!framebufferInvalidated) return;
    const uint32_t startedAt = millis();
    renderer.clearScreen();
    const auto layout = UiLayout::from(renderer);
    const auto& metrics = UITheme::getInstance().getMetrics();
    GUI.drawHeader(renderer, Rect{layout.content.x, metrics.topPadding, layout.content.width, metrics.headerHeight},
                   tr(STR_GENERATE_ALL_CACHE));
    const int centerOffset = layout.content.x + layout.content.width / 2 - renderer.getScreenWidth() / 2;
    const int centerY = renderer.getScreenHeight() / 2;
    renderer.drawCenteredTextOffset(UI_10_FONT_ID, centerY, tr(STR_GENERATING_ALL_CACHE), true, centerOffset);
    renderer.drawCenteredTextOffset(UI_10_FONT_ID, centerY + 25, tr(STR_CACHE_CANCEL_HINT_LINE1), true, centerOffset);
    renderer.drawCenteredTextOffset(UI_10_FONT_ID, centerY + 45, tr(STR_CACHE_CANCEL_HINT_LINE2), true, centerOffset);
    const auto cancelLabels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, cancelLabels.btn1, cancelLabels.btn2, cancelLabels.btn3, cancelLabels.btn4);
    popupRect = GUI.drawProgressPopup(renderer, tr(STR_GENERATING_ALL_CACHE), progressDetail.c_str());
    GUI.updateProgressPopup(renderer, popupRect, progressDetail.c_str(), lastDisplayedProgress);
    progressDisplayMs += millis() - startedAt;
    framebufferInvalidated = false;
    LOG_DBG("IMEM", "Restored progress UI after JPEG framebuffer loan");
  };

  bool cancelled = false;
  bool storageFailure = false;

  // Calculate viewport dimensions (screenMargin depends on writing direction, resolved per-book below)
  // Use a placeholder margin here; it will be recalculated per book after resolving isVertical.
  int orientedMarginTop = 0, orientedMarginRight = 0, orientedMarginBottom = 0, orientedMarginLeft = 0;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  const int baseMarginTop = orientedMarginTop;
  const int baseMarginRight = orientedMarginRight;
  const int baseMarginBottom = orientedMarginBottom;
  const int baseMarginLeft = orientedMarginLeft;
  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  for (int bookIdx = 0; bookIdx < totalCount; bookIdx++) {
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
    // Declared before the EPUB and other book-local objects so destruction
    // logs after they release memory, including continue/break paths.
    BatchBookMemoryProbe memoryProbe{bookIdx + 1};
    logBatchMemory("book-start", bookIdx + 1);
#endif
    std::string epubPath;
    if (!epubFiles.next(epubPath)) {
      LOG_ERR("GENALL", "Could not read temporary EPUB list at book %d", bookIdx + 1);
      storageFailure = true;
      break;
    }
    const uint32_t bookStartedAt = millis();
    uint32_t sectionBuildMs = 0;
    uint32_t pixelCacheMs = 0;
    int sectionCacheHits = 0;
    int generatedSections = 0;
    int generatedPixelCaches = 0;
    int cachedPixelPagesScanned = 0;
    LOG_DBG("GENALL", "Processing %d/%d: %s", bookIdx + 1, totalCount, epubPath.c_str());

    const int progress = (bookIdx * 100) / totalCount;
    if (progressPolicy.shouldUpdate(bookIdx, progress, millis())) {
      progressDetail =
          std::string(tr(STR_CACHE_BOOK)) + " " + std::to_string(bookIdx + 1) + "/" + std::to_string(totalCount);
      const uint32_t displayStartedAt = millis();
      GUI.updateProgressPopup(renderer, popupRect, progressDetail.c_str(), progress);
      progressDisplayMs += millis() - displayStartedAt;
      lastDisplayedProgress = progress;
      progressPolicy.displayed(bookIdx, progress, millis());
    }

    if (controls.shouldCancel(renderer)) {
      LOG_DBG("GENALL", "Cancelled by user at book %d/%d", bookIdx + 1, totalCount);
      cancelled = true;
      break;
    }

    // CSS is parsed during load(), before section-level reclamation runs.
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
    logBatchMemory("before-font-release", bookIdx + 1);
#endif
    if (auto* fontCache = renderer.getFontCacheManager()) {
      fontCache->clearCache();
      fontCache->freeKernLigatureData();
      fontCache->releaseSdFontCaches();
    }
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
    logBatchMemory("after-font-release", bookIdx + 1);
#endif
    // Load EPUB
    auto epub = std::make_shared<Epub>(epubPath, "/.crosspoint");
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
    logBatchMemory("before-load", bookIdx + 1);
#endif
    if (!epub->load(true, SETTINGS.embeddedStyle == CrossPointSettings::CROSSPOINT_STYLE)) {
      LOG_ERR("GENALL", "Failed to load: %s", epubPath.c_str());
      continue;
    }
#if defined(BATCH_CSS_MEMORY_DIAGNOSTICS)
    logBatchMemory("after-load", bookIdx + 1);
#endif

    const int spineCount = epub->getSpineItemsCount();
    if (spineCount <= 0) continue;
    epub->clearFullCacheGeneratedMarker();
    if (SETTINGS.embeddedStyle != CrossPointSettings::CROSSPOINT_STYLE &&
        (!epub->getCssParser() || !epub->getCssParser()->validateCache())) {
      LOG_ERR("GENALL", "CSS not ready; leaving book incomplete: %s", epubPath.c_str());
      continue;
    }

    const uint32_t pixelPreflightStartedAt = millis();
    const auto pixelPreflight = inspectPixelCaches(epub->getCachePath(), spineCount);
    const uint32_t pixelPreflightMs = millis() - pixelPreflightStartedAt;
    pixelCacheMs += pixelPreflightMs;
    if (pixelPreflight.complete) {
      LOG_DBG("GENALL",
              "Raster cache preflight: sources=%d, valid=%d, missing/invalid=%d, removed-zero-length=%d, time=%lu ms",
              pixelPreflight.sourceCount, pixelPreflight.validCacheCount, pixelPreflight.missingOrInvalidCacheCount,
              pixelPreflight.removedZeroLengthSourceCount, pixelPreflightMs);
    } else {
      LOG_DBG("GENALL", "Raster cache preflight incomplete; using cached-page fallback (%lu ms)", pixelPreflightMs);
    }

    // Generate cover thumbnail
    const int coverHeight = UITheme::getInstance().getMetrics().homeCoverHeight;
    epub->generateThumbBmp(coverHeight);

    // Resolve writing mode
    bool isVertical = false;
    if (SETTINGS.writingMode == CrossPointSettings::WM_VERTICAL) {
      isVertical = true;
    } else if (SETTINGS.writingMode == CrossPointSettings::WM_HORIZONTAL) {
      isVertical = false;
    } else {
      isVertical = epub->isPageProgressionRtl() && (epub->getLanguage() == "ja" || epub->getLanguage() == "jpn" ||
                                                    epub->getLanguage() == "zh" || epub->getLanguage() == "zho");
    }

    const float lineCompression = SETTINGS.getReaderLineCompression(isVertical);
    renderer.setVerticalCharSpacing(SETTINGS.getVerticalCharSpacingPercent());

    auto* fcm = renderer.getFontCacheManager();
    if (fcm) {
      fcm->clearCache();
      fcm->freeKernLigatureData();
    }

    const auto& ds = SETTINGS.getDirectionSettings(isVertical);
    ensureSdFontLoaded(isVertical);
    configureRubyFont(isVertical);
    configureSmallFont(isVertical);

    // Calculate viewport dimensions with direction-specific margins
    const int bmTop = baseMarginTop + ds.screenMargin;
    const int bmRight = baseMarginRight + ds.screenMargin;
    const int bmLeft = baseMarginLeft + ds.screenMargin;
    const int bmBottom =
        baseMarginBottom + std::max(static_cast<int>(ds.screenMargin), getStatusBarContentReservation(statusBarHeight));
    const uint16_t viewportWidth = screenWidth - bmLeft - bmRight;
    const uint16_t viewportHeight = screenHeight - bmTop - bmBottom;

    const int headingFontIds[6] = {
        SETTINGS.getHeadingFontId(1, isVertical), SETTINGS.getHeadingFontId(2, isVertical), 0, 0, 0, 0};
    bool allSectionsReady = true;

    for (int i = 0; i < spineCount; i++) {
      if (controls.shouldCancel(renderer)) {
        LOG_DBG("GENALL", "Cancelled at section %d/%d of book %d/%d", i, spineCount, bookIdx + 1, totalCount);
        cancelled = true;
        allSectionsReady = false;
        break;
      }
      const int bookProgress = (i * 80) / spineCount;
      const int overallProgress = (bookIdx * 100 + bookProgress) / totalCount;
      if (progressPolicy.shouldUpdate(bookIdx, overallProgress, millis())) {
        progressDetail = std::string(tr(STR_CACHE_BOOK)) + " " + std::to_string(bookIdx + 1) + "/" +
                         std::to_string(totalCount) + "  " + tr(STR_CACHE_CHAPTER) + " " + std::to_string(i + 1) + "/" +
                         std::to_string(spineCount);
        const uint32_t displayStartedAt = millis();
        GUI.updateProgressPopup(renderer, popupRect, progressDetail.c_str(), overallProgress);
        progressDisplayMs += millis() - displayStartedAt;
        lastDisplayedProgress = overallProgress;
        progressPolicy.displayed(bookIdx, overallProgress, millis());
      }
      Section sec(epub, i, renderer);
      const bool sectionCached =
          sec.loadSectionFile(SETTINGS.getReaderFontId(isVertical), SETTINGS.getTableFontId(isVertical),
                              lineCompression, ds.extraParagraphSpacing, ds.paragraphAlignment, viewportWidth,
                              viewportHeight, ds.hyphenationEnabled, ds.firstLineIndent, SETTINGS.embeddedStyle,
                              SETTINGS.imageRendering, isVertical, ds.charSpacing, ds.tateChuYokoMaxDigits);
      if (sectionCached) {
        sectionCacheHits++;
        // Read cached pages only when the directory preflight found a missing or
        // invalid raster-image cache. If preflight failed, preserve the fallback probe.
        bool needsPixelPageScan = pixelPreflight.complete && pixelPreflight.sectionsNeedingPageScan[i];
        if (!pixelPreflight.complete) {
          const std::string imagePrefix = epub->getCachePath() + "/img_" + std::to_string(i) + "_0";
          needsPixelPageScan = Storage.exists((imagePrefix + ".png").c_str()) ||
                               Storage.exists((imagePrefix + ".jpg").c_str()) ||
                               Storage.exists((imagePrefix + ".jpeg").c_str());
        }
        if (needsPixelPageScan) {
          const uint32_t pixelStartedAt = millis();
          generatedPixelCaches += pregeneratePixelCachesFromCachedSection(
              sec, renderer, bmLeft, bmTop, cachedPixelPagesScanned, framebufferInvalidated);
          pixelCacheMs += millis() - pixelStartedAt;
          restoreProgress();
        }
      } else {
        const uint32_t sectionStartedAt = millis();
        const int cssBodyFontIds[4] = {SETTINGS.getReaderFontIdForSize(isVertical, CrossPointSettings::SMALL),
                                       SETTINGS.getReaderFontIdForSize(isVertical, CrossPointSettings::MEDIUM),
                                       SETTINGS.getReaderFontIdForSize(isVertical, CrossPointSettings::LARGE),
                                       SETTINGS.getReaderFontIdForSize(isVertical, CrossPointSettings::EXTRA_LARGE)};
        if (!sec.createSectionFile(
                SETTINGS.getReaderFontId(isVertical), lineCompression, ds.extraParagraphSpacing, ds.paragraphAlignment,
                viewportWidth, viewportHeight, ds.hyphenationEnabled, ds.firstLineIndent, SETTINGS.embeddedStyle,
                SETTINGS.imageRendering, isVertical, ds.charSpacing, ds.tateChuYokoMaxDigits, nullptr, headingFontIds,
                SETTINGS.getTableFontId(isVertical), cssBodyFontIds, nullptr,
                [this, &generatedPixelCaches, &pixelCacheMs, &framebufferInvalidated, &restoreProgress, bmLeft,
                 bmTop](const Page& page) {
                  const uint32_t pixelStartedAt = millis();
                  generatedPixelCaches += pregeneratePixelCaches(page, renderer, bmLeft, bmTop, framebufferInvalidated);
                  pixelCacheMs += millis() - pixelStartedAt;
                  restoreProgress();
                },
                [&controls, this] { return controls.shouldCancel(renderer); }, true)) {
          LOG_ERR("GENALL", "Failed section %d of %s", i, epubPath.c_str());
          allSectionsReady = false;
          const auto failureReason = sec.getLastCreateFailureReason();
          if (failureReason == Section::CreateFailureReason::Cancelled || controls.shouldCancel(renderer)) {
            cancelled = true;
            break;
          }
          if (failureReason == Section::CreateFailureReason::CssUnavailable) break;
          if (failureReason == Section::CreateFailureReason::StorageIo) {
            LOG_ERR("GENALL", "Stopping cache generation after SD I/O failure at section %d of %s", i,
                    epubPath.c_str());
            storageFailure = true;
            break;
          }
          continue;
        }
        sectionBuildMs += millis() - sectionStartedAt;
        generatedSections++;
      }
    }

    if (cancelled || storageFailure) break;

    if (allSectionsReady) {
      if (!epub->markFullCacheGenerated()) {
        LOG_ERR("GENALL", "Could not publish completion marker: %s", epubPath.c_str());
        storageFailure = true;
      }
    } else {
      LOG_DBG("GENALL", "Cache incomplete for %s; a later run will resume it", epubPath.c_str());
    }

    LOG_DBG("GENALL",
            "Book timing: total=%lu ms, section-build=%lu ms (%d generated, %d cached), PXC=%lu ms (%d images, %d "
            "cached pages scanned)",
            millis() - bookStartedAt, sectionBuildMs, generatedSections, sectionCacheHits, pixelCacheMs,
            generatedPixelCaches, cachedPixelPagesScanned);
    if (cancelled || storageFailure) break;
  }

  if (!cancelled && !storageFailure) {
    const uint32_t finalDisplayStartedAt = millis();
    progressDetail = std::string(tr(STR_CACHE_COMPLETE));
    GUI.updateProgressPopup(renderer, popupRect, progressDetail.c_str(), 100);
    progressDisplayMs += millis() - finalDisplayStartedAt;
  }

  if (!storageFailure && !summarizeCacheStatuses(epubFiles)) {
    LOG_ERR("GENALL", "Could not read EPUB list for status summary");
    storageFailure = true;
  }

  LOG_DBG("GENALL", "Cache generation completed in %lu ms (progress display: %lu ms)", millis() - generationStartedAt,
          progressDisplayMs);
#if defined(CACHE_STORAGE_FAULT_INJECTION)
  if (storageFailure) {
    LOG_INF("SDFI", "event=safe_stop reason=storage_io");
  }
#endif
  state = storageFailure ? FAILED : (cancelled ? INTERRUPTED : SUCCESS);
  requestUpdate();
}

void GenerateAllCacheActivity::loop() {
  if (CacheGenerationControls::consumeCancellationRelease(mappedInput)) return;
  if (state == CONFIRMING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      {
        RenderLock lock(*this);
        state = GENERATING;
      }
      requestUpdateAndWait();
      generateAllCaches();
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }

  if (state == SUCCESS || state == INTERRUPTED || state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }
}
