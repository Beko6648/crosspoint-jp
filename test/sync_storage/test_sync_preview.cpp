#include <algorithm>
#include <iostream>

#include "sync/ReviewNavigation.h"
#include "sync/SnapshotPreview.h"
using namespace yomuka::sync;
unsigned checks = 0;
#define CHECK(x)                                   \
  do {                                             \
    ++checks;                                      \
    if (!(x)) {                                    \
      std::cerr << __LINE__ << ": " << #x << "\n"; \
      return 1;                                    \
    }                                              \
  } while (0)
bool has(const std::vector<std::string>& lines, const std::string& text) {
  return std::any_of(lines.begin(), lines.end(),
                     [&](const auto& line) { return line.find(text) != std::string::npos; });
}
int main() {
  JsonDocument a, b;
  a["updatedAt"] = 200;
  b["updatedAt"] = 100;
  a["data"]["vertical"]["rubyOffsetX"] = -16;
  a["data"]["vertical"]["font"]["family"] = "noto-sans";
  a["data"]["vertical"]["font"]["sdFamilyName"] = "日本語";
  b["data"].to<JsonObject>();
  auto lines = previewUnit(2, a, b, 3, true);
  CHECK(has(lines, "すべて解除"));
  CHECK(has(lines, "ルビ横位置: -16 → 全体設定を継承"));
  CHECK(has(lines, "SD: 日本語"));
  CHECK(has(lines, "受信の更新日時が古い"));
  b["updatedAt"] = 0;
  lines = previewUnit(2, a, b, 3, true);
  CHECK(has(lines, "新旧は判定しません"));
  CHECK(!has(lines, "古いデータ"));
  b.set(a);
  lines = previewUnit(2, a, b, 3, true);
  CHECK(has(lines, "設定値の変更なし"));
  auto font = b["data"]["vertical"]["font"];
  font["family"] = "noto-serif";
  lines = previewUnit(2, a, b, 3, true);
  CHECK(has(lines, "Noto Serif"));
  a["data"].to<JsonArray>().add<JsonObject>()["summary"] = "消えるしおり";
  b["data"].to<JsonArray>();
  lines = previewUnit(1, a, b, 3, true);
  CHECK(has(lines, "すべて削除"));
  CHECK(has(lines, "消えるしおり"));
  auto mark = b["data"].add<JsonObject>();
  mark["spineIndex"] = 1;
  mark["chapterPage"] = 4;
  mark["summary"] = std::string(200, 'a');
  lines = previewUnit(1, a, b, 3, true);
  CHECK(has(lines, "章 2 / ページ 5"));
  CHECK(has(lines, std::string(64, 'a') + "…"));
  a["data"].to<JsonObject>();
  b["data"].to<JsonObject>();
  b["data"]["spineIndex"] = 3;
  b["data"]["percent"] = nullptr;
  b["data"]["finished"] = false;
  lines = previewUnit(0, a, b, 3, true);
  CHECK(has(lines, "本の終端"));
  CHECK(has(lines, "不明"));
  b["data"]["seconds"] = 1000;
  b["data"]["sessionCount"] = 7;
  b["data"]["lastReadAt"] = 0;
  b["data"]["finishedAt"] = 0;
  lines = previewUnit(3, a, b, 3, true);
  CHECK(has(lines, "1000"));
  CHECK(has(lines, "読書回数"));
  CHECK(has(lines, "最終読書日時"));
  CHECK(has(lines, "完読日時"));
  CHECK(has(lines, "加算なし"));
  CHECK(has(lines, "累計は変えません"));
  const char* global[] = {"writingMode", "orientation", "bookStyle", "imageRendering", "invertImages"};
  const char* fields[] = {
      "font",         "fontSize",        "lineSpacing", "charSpacing", "paragraphAlignment", "extraParagraphSpacing",
      "screenMargin", "firstLineIndent", "rubyEnabled", "rubyOffsetX", "rubyOffsetY",        "tateChuYokoMaxDigits"};
  const char* values[] = {"vertical", "landscape-cw", "epub", "suppress", nullptr};
  for (unsigned i = 0; i < 5; ++i) {
    a["data"].to<JsonObject>();
    b["data"].to<JsonObject>();
    if (values[i])
      b["data"][global[i]] = values[i];
    else
      b["data"][global[i]] = true;
    lines = previewUnit(2, a, b, 3, true);
    CHECK(!has(lines, "設定値の変更なし"));
  }
  for (const char* direction : {"vertical", "horizontal"})
    for (const char* field : fields) {
      a["data"].to<JsonObject>();
      b["data"].to<JsonObject>();
      if (std::string(field) == "font") {
        b["data"][direction][field]["family"] = "noto-sans";
        b["data"][direction][field]["sdFamilyName"] = "";
      } else
        b["data"][direction][field] = 1;
      std::string saved;
      serializeJson(b, saved);
      lines = previewUnit(2, a, b, 3, true);
      CHECK(!has(lines, "設定値の変更なし"));
      std::string after;
      serializeJson(b, after);
      CHECK(after == saved);
      lines = previewUnit(2, a, b, 3, false);
      CHECK(!has(lines, "現在の更新"));
    }
  // Real confirmation navigation: Confirm never advances or executes a comparison page.
  for (int rows : {10, 14, 16, 21})
    for (int length : {1, 10, 11, 14, 16, 21, 22, 100, 512}) {
      const int last = finalReviewPage(length, rows);
      CHECK(last >= 1);
      int page = 0;
      CHECK(reviewInput(page, last, ReviewButton::Previous).page == 0);
      for (; page < last;) {
        const auto confirm = reviewInput(page, last, ReviewButton::Confirm);
        CHECK(confirm.page == page && !confirm.execute);
        const auto next = reviewInput(page, last, ReviewButton::Next);
        CHECK(!next.execute && next.page == page + 1);
        page = next.page;
      }
      CHECK(reviewInput(page, last, ReviewButton::Confirm).execute);
      CHECK(reviewInput(page, last, ReviewButton::Next).page == page);
      const auto back = reviewInput(page, last, ReviewButton::Previous);
      CHECK(back.page == page - 1 && !back.execute);
      CHECK(!reviewInput(back.page, last, ReviewButton::Confirm).execute);
    }
  std::cout << "PASS preview: " << checks << " checks\n";
}
