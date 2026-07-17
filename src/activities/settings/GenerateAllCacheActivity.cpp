#include "GenerateAllCacheActivity.h"

#include <Epub.h>
#include <Epub/Section.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SdCardFontGlobals.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

// E-paper progress redraws are expensive (~670 ms in the measured run).
// Quarter-step updates keep useful feedback without dominating cache creation.
constexpr int CACHE_PROGRESS_STEP_PERCENT = 25;

// Recursively scan a directory for EPUB files
void findEpubFiles(const char* dirPath, std::vector<std::string>& results) {
  auto dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
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
      findEpubFiles(fullPath.c_str(), results);
    } else {
      if (FsHelpers::hasEpubExtension(std::string_view(name))) {
        results.push_back(fullPath);
      }
      file.close();
    }
  }
  dir.close();
}

}  // namespace

void GenerateAllCacheActivity::onEnter() {
  Activity::onEnter();
  state = CONFIRMING;
  requestUpdate();
}

void GenerateAllCacheActivity::onExit() { Activity::onExit(); }

void GenerateAllCacheActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GENERATE_ALL_CACHE));

  if (state == CONFIRMING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_GENERATE_CACHE), true);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_GENERATE_CACHE_NOTE), true);

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == GENERATING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_GENERATING_ALL_CACHE));
    renderer.displayBuffer();
    return;
  }

  if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CACHE_GENERATED), true, EpdFontFamily::BOLD);
    std::string resultText = std::to_string(processedCount) + " " + std::string(tr(STR_BOOKS_PROCESSED));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, resultText.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

void GenerateAllCacheActivity::generateAllCaches() {
  const uint32_t generationStartedAt = millis();
  LOG_DBG("GENALL", "Scanning for EPUB files...");

  const uint32_t scanStartedAt = millis();
  std::vector<std::string> epubFiles;
  findEpubFiles("/", epubFiles);
  LOG_DBG("GENALL", "EPUB scan completed in %lu ms", millis() - scanStartedAt);

  totalCount = epubFiles.size();
  processedCount = 0;

  LOG_DBG("GENALL", "Found %d EPUB files", totalCount);

  if (totalCount == 0) {
    state = SUCCESS;
    requestUpdate();
    return;
  }

  // Show progress popup
  const uint32_t initialDisplayStartedAt = millis();
  Rect popupRect = GUI.drawPopup(renderer, tr(STR_GENERATING_ALL_CACHE));
  uint32_t progressDisplayMs = millis() - initialDisplayStartedAt;
  int lastDisplayedProgress = 0;

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
    const auto& epubPath = epubFiles[bookIdx];
    const uint32_t bookStartedAt = millis();
    uint32_t sectionBuildMs = 0;
    uint32_t imageCacheMs = 0;
    int sectionCacheHits = 0;
    int generatedSections = 0;
    int generatedImageCaches = 0;
    LOG_DBG("GENALL", "Processing %d/%d: %s", bookIdx + 1, totalCount, epubPath.c_str());

    // Update progress
    const int progress = (bookIdx * 100) / totalCount;
    if (progress >= lastDisplayedProgress + CACHE_PROGRESS_STEP_PERCENT) {
      const uint32_t displayStartedAt = millis();
      GUI.fillPopupProgress(renderer, popupRect, progress);
      progressDisplayMs += millis() - displayStartedAt;
      lastDisplayedProgress = progress;
    }

    // Check for cancel (button held)
    const int adc1 = analogRead(1);
    const int adc2 = analogRead(2);
    if (adc1 < 3800 || adc2 < 3800) {
      LOG_DBG("GENALL", "Cancelled by user at book %d/%d", bookIdx + 1, totalCount);
      break;
    }

    // Load EPUB
    auto epub = std::make_shared<Epub>(epubPath, "/.crosspoint");
    if (!epub->load(true, SETTINGS.embeddedStyle == CrossPointSettings::CROSSPOINT_STYLE)) {
      LOG_ERR("GENALL", "Failed to load: %s", epubPath.c_str());
      continue;
    }

    const int spineCount = epub->getSpineItemsCount();
    if (spineCount <= 0) continue;

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

    // Calculate viewport dimensions with direction-specific margins
    const int bmTop = baseMarginTop + ds.screenMargin;
    const int bmRight = baseMarginRight + ds.screenMargin;
    const int bmLeft = baseMarginLeft + ds.screenMargin;
    const int bmBottom = baseMarginBottom + std::max(ds.screenMargin, statusBarHeight);
    const uint16_t viewportWidth = screenWidth - bmLeft - bmRight;
    const uint16_t viewportHeight = screenHeight - bmTop - bmBottom;

    const int headingFontIds[6] = {
        SETTINGS.getHeadingFontId(1, isVertical), SETTINGS.getHeadingFontId(2, isVertical), 0, 0, 0, 0};

    for (int i = 0; i < spineCount; i++) {
      Section sec(epub, i, renderer);
      const bool sectionCached = sec.loadSectionFile(
          SETTINGS.getReaderFontId(isVertical), lineCompression, ds.extraParagraphSpacing, ds.paragraphAlignment,
          viewportWidth, viewportHeight, ds.hyphenationEnabled, ds.firstLineIndent, SETTINGS.embeddedStyle,
          SETTINGS.imageRendering, isVertical, ds.charSpacing);
      if (sectionCached) {
        sectionCacheHits++;
      } else {
        const uint32_t sectionStartedAt = millis();
        if (!sec.createSectionFile(SETTINGS.getReaderFontId(isVertical), lineCompression, ds.extraParagraphSpacing,
                                   ds.paragraphAlignment, viewportWidth, viewportHeight, ds.hyphenationEnabled,
                                   ds.firstLineIndent, SETTINGS.embeddedStyle, SETTINGS.imageRendering, isVertical,
                                   ds.charSpacing, nullptr, headingFontIds, SETTINGS.getTableFontId(isVertical))) {
          LOG_ERR("GENALL", "Failed section %d of %s", i, epubPath.c_str());
          continue;
        }
        sectionBuildMs += millis() - sectionStartedAt;
        generatedSections++;
      }

      // Generate image BMP caches
      const std::string imgPrefix = epub->getCachePath() + "/img_" + std::to_string(i) + "_";
      for (int j = 0;; j++) {
        std::string jpgPath = imgPrefix + std::to_string(j) + ".jpg";
        if (!Storage.exists(jpgPath.c_str())) {
          jpgPath = imgPrefix + std::to_string(j) + ".jpeg";
          if (!Storage.exists(jpgPath.c_str())) break;
        }

        const size_t dotPos = jpgPath.rfind('.');
        const std::string bmpCachePath = jpgPath.substr(0, dotPos) + ".pxc4.bmp";
        if (Storage.exists(bmpCachePath.c_str())) {
          FsFile existingBmp;
          if (Storage.openFileForRead("GEN", bmpCachePath, existingBmp)) {
            const size_t existingSize = existingBmp.size();
            existingBmp.close();
            if (existingSize > 70) continue;
            LOG_DBG("GENALL", "Removing incomplete JPEG cache: %s (%lu bytes)", bmpCachePath.c_str(),
                    static_cast<unsigned long>(existingSize));
            Storage.remove(bmpCachePath.c_str());
          }
        }

        FsFile jpegFile, bmpFile;
        if (Storage.openFileForRead("GEN", jpgPath, jpegFile) &&
            Storage.openFileForWrite("GEN", bmpCachePath, bmpFile)) {
          const uint32_t imageStartedAt = millis();
          const bool success =
              JpegToBmpConverter::jpegFileToBmpStreamWithSize(jpegFile, bmpFile, viewportWidth, viewportHeight);
          imageCacheMs += millis() - imageStartedAt;
          jpegFile.close();
          bmpFile.close();
          if (success) generatedImageCaches++;
        }
      }
    }

    LOG_DBG("GENALL",
            "Book timing: total=%lu ms, section-build=%lu ms (%d generated, %d cached), JPEG-BMP=%lu ms (%d images)",
            millis() - bookStartedAt, sectionBuildMs, generatedSections, sectionCacheHits, imageCacheMs,
            generatedImageCaches);
    processedCount++;
  }

  const uint32_t finalDisplayStartedAt = millis();
  GUI.fillPopupProgress(renderer, popupRect, 100);
  progressDisplayMs += millis() - finalDisplayStartedAt;

  LOG_DBG("GENALL", "Cache generation completed in %lu ms (progress display: %lu ms)",
          millis() - generationStartedAt, progressDisplayMs);
  state = SUCCESS;
  requestUpdate();
}

void GenerateAllCacheActivity::loop() {
  if (state == CONFIRMING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      {
        RenderLock lock(*this);
        state = GENERATING;
      }
      requestUpdateAndWait();
      generateAllCaches();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }

  if (state == SUCCESS) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }
}
