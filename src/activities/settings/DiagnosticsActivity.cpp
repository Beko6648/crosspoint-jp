#include "DiagnosticsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <ctime>
#include <string_view>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr const char* kDiagnosticsDirectory = "/.crosspoint/diagnostics";

int countReadingCacheDirectories() {
  auto root = Storage.open("/.crosspoint");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }

  int count = 0;
  char name[128];
  for (auto entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    const bool isDirectory = entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    if (!isDirectory) continue;

    const std::string_view entryName(name);
    if (entryName.rfind("epub_", 0) == 0 || entryName.rfind("xtc_", 0) == 0 || entryName.rfind("txt_", 0) == 0) {
      ++count;
    }
  }
  root.close();
  return count;
}

std::vector<std::string> splitLogLines(const std::string& logs) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < logs.size()) {
    const size_t end = logs.find('\n', start);
    const size_t length = end == std::string::npos ? logs.size() - start : end - start;
    if (length > 0) lines.push_back(logs.substr(start, length));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}

std::string makeReportPath() {
  const time_t now = time(nullptr);
  if (now >= 1704067200) {
    struct tm timeInfo {};
    localtime_r(&now, &timeInfo);
    char filename[48];
    snprintf(filename, sizeof(filename), "report_%04d%02d%02d_%02d%02d%02d.txt", timeInfo.tm_year + 1900,
             timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    return std::string(kDiagnosticsDirectory) + "/" + filename;
  }
  return std::string(kDiagnosticsDirectory) + "/report_boot_" + std::to_string(millis()) + ".txt";
}

}  // namespace

void DiagnosticsActivity::onEnter() {
  Activity::onEnter();
  collectSnapshot();
  requestUpdate();
}

void DiagnosticsActivity::collectSnapshot() {
  sdReady = Storage.ready();
  freeHeap = ESP.getFreeHeap();
  maxAllocHeap = ESP.getMaxAllocHeap();
  minFreeHeap = ESP.getMinFreeHeap();
  cacheDirectoryCount = sdReady ? countReadingCacheDirectories() : 0;
  recentLogs = getLastLogs();
  recentLogLines = splitLogLines(recentLogs);
}

bool DiagnosticsActivity::saveReport() {
  if (!sdReady || !Storage.ensureDirectoryExists(kDiagnosticsDirectory)) return false;

  savedReportPath = makeReportPath();
  auto file = Storage.open(savedReportPath.c_str(), O_WRITE | O_CREAT | O_TRUNC);
  if (!file) return false;

  file.printf("Yomuka diagnostics\n");
  file.printf("version=%s\n", CROSSPOINT_VERSION);
  file.printf("device=%s\n", gpio.deviceIsX3() ? "X3" : "X4");
  file.printf("sd_ready=%s\n", sdReady ? "true" : "false");
  file.printf("free_heap=%lu\n", static_cast<unsigned long>(freeHeap));
  file.printf("max_alloc_heap=%lu\n", static_cast<unsigned long>(maxAllocHeap));
  file.printf("min_free_heap=%lu\n", static_cast<unsigned long>(minFreeHeap));
  file.printf("reading_cache_directories=%d\n", cacheDirectoryCount);
  file.printf("captured_millis=%lu\n", static_cast<unsigned long>(millis()));
  file.print("\nRecent logs:\n");
  file.print(recentLogs.c_str());
  file.close();
  LOG_INF("DIAG", "Saved diagnostics report: %s", savedReportPath.c_str());
  return true;
}

void DiagnosticsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    collectSnapshot();
    saveResult = saveReport() ? SaveResult::Saved : SaveResult::Failed;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    page = page == Page::Overview ? Page::Logs : Page::Overview;
    requestUpdate();
  }
}

void DiagnosticsActivity::renderOverview(const int x, int y, const int contentWidth, const int lineHeight) {
  const auto drawLine = [this, x, &y, lineHeight](const std::string& text) {
    renderer.drawText(UI_10_FONT_ID, x, y, text.c_str());
    y += lineHeight;
  };
  drawLine(std::string("Yomuka: ") + CROSSPOINT_VERSION);
  drawLine(std::string("Device: ") + (gpio.deviceIsX3() ? "X3" : "X4"));
  drawLine(std::string("SD: ") + (sdReady ? "ready" : "unavailable"));
  drawLine("Heap: " + std::to_string(freeHeap));
  drawLine("Max alloc: " + std::to_string(maxAllocHeap));
  drawLine("Min free: " + std::to_string(minFreeHeap));
  drawLine("Cache dirs: " + std::to_string(cacheDirectoryCount));
  drawLine("Recent logs: " + std::to_string(recentLogLines.size()));

  y += lineHeight / 2;
  if (saveResult == SaveResult::Saved) {
    renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_DIAGNOSTICS_REPORT_SAVED));
    y += lineHeight;
    for (const auto& wrapped : renderer.wrappedText(UI_10_FONT_ID, savedReportPath.c_str(), contentWidth, 2)) {
      renderer.drawText(UI_10_FONT_ID, x, y, wrapped.c_str());
      y += lineHeight;
    }
  } else if (saveResult == SaveResult::Failed) {
    renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_DIAGNOSTICS_REPORT_FAILED));
  }
}

void DiagnosticsActivity::renderLogs(const int x, int y, const int contentWidth, const int lineHeight) {
  if (recentLogLines.empty()) {
    renderer.drawText(UI_10_FONT_ID, x, y, "(no recent logs)");
    return;
  }

  const size_t firstLine = recentLogLines.size() > 4 ? recentLogLines.size() - 4 : 0;
  for (size_t i = firstLine; i < recentLogLines.size(); ++i) {
    for (const auto& wrapped : renderer.wrappedText(UI_10_FONT_ID, recentLogLines[i].c_str(), contentWidth, 2)) {
      renderer.drawText(UI_10_FONT_ID, x, y, wrapped.c_str());
      y += lineHeight;
    }
    y += 2;
  }
}

void DiagnosticsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int x = metrics.contentSidePadding;
  const int contentWidth = pageWidth - 2 * x;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DIAGNOSTICS));
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.drawText(UI_10_FONT_ID, x, y,
                    page == Page::Overview ? tr(STR_DIAGNOSTICS_OVERVIEW) : tr(STR_DIAGNOSTICS_RECENT_LOGS));
  y += lineHeight + metrics.verticalSpacing;

  if (page == Page::Overview) {
    renderOverview(x, y, contentWidth, lineHeight);
  } else {
    renderLogs(x, y, contentWidth, renderer.getLineHeight(UI_10_FONT_ID));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DIAGNOSTICS_SAVE_REPORT),
                                             tr(STR_DIAGNOSTICS_RECENT_LOGS), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
