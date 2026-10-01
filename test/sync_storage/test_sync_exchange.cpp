#include <HalStorage.h>

#include <ctime>
#include <iostream>

#include "sync/BookmarkStorage.h"
#include "sync/ProgressStorage.h"
#include "sync/SnapshotExchange.h"
using namespace yomuka::sync;
unsigned checks = 0;
#define CHECK(c)                                   \
  do {                                             \
    ++checks;                                      \
    if (!(c)) {                                    \
      std::cerr << __LINE__ << ": " << #c << "\n"; \
      return 1;                                    \
    }                                              \
  } while (0)
bool layout(const Progress& p, const BookReaderSettings::Override*, Progress& out, void*) {
  ProjectedProgress target;
  if (projectProgressForBook(p, 3, {true, 5}, target) != ProgressError::None) return false;
  out = p;
  out.chapterPage = target.chapterPage;
  out.chapterPageCount = target.chapterPageCount;
  if (!out.percent) out.percent = 50;
  if (!out.finished) out.finished = false;
  return true;
}
int main() {
  testUnixTime = 1710000000;
  const ExchangeBook book{1, 3, "/same.epub", "Same", "Author"};
  const std::string path = "/.crosspoint/books/0000000000000001/progress.bin";
  uint8_t raw[] = {1, 0, 2, 0, 10, 0, 0, 50};
  CHECK(saveProgress(path, raw, 8));
  BookmarkEntry bookmark;
  bookmark.summary = "bookmark";
  bookmark.spineIndex = 1;
  bookmark.chapterPage = 2;
  bookmark.chapterPageCount = 10;
  bookmark.percentage = .5f;
  CHECK(saveBookmarks("/.crosspoint/books/0000000000000001/bookmarks.json", {bookmark}));
  BookReaderSettings::Override settings;
  settings.fields = BookReaderSettings::WritingMode;
  settings.writingMode = 2;
  CHECK(BookReaderSettings::save(1, settings));
  CHECK(BookReaderSettings::save(2, settings));
  ReadingHistoryStore history;
  history.beginSession(book.path, book.title, book.author, book.id);
  Storage.nowMs = 2000;
  CHECK(history.endSession());
  JsonDocument snapshot;
  CHECK(exportSnapshot(book, All, history, snapshot) == ExchangeError::None);
  CHECK(snapshot["units"]["readerSettings"]["data"].size() == 1);
  CHECK(snapshot["units"]["readerSettings"]["data"]["writingMode"] == "vertical");
  // Candidate listing uses content identity, including renamed files.
  std::string exported;
  serializeJson(snapshot, exported);
  const std::string renamed = "/YomukaSync/renamed.json";
  Storage.put(renamed, exported);
  JsonDocument listed;
  CHECK(readSnapshotForBook(renamed, book, listed) == ExchangeError::None);
  CHECK(listed["bookId"] == "0000000000000001");
  listed["sentinel"] = 42;
  const auto fileState = Storage.snapshot();
  const ExchangeBook other{2, 3, "/other.epub", "Other", "Author"};
  CHECK(readSnapshotForBook(renamed, other, listed) == ExchangeError::Invalid);
  CHECK(listed["sentinel"] == 42 && Storage.snapshot() == fileState);
  // A filename naming book 2 cannot override the book 1 identity in its contents.
  Storage.put(exchangeFilePath(2), exported);
  CHECK(readSnapshotForBook(exchangeFilePath(2), book, listed) == ExchangeError::None);
  JsonDocument different;
  different.set(snapshot);
  different["bookId"] = "0000000000000002";
  std::string text;
  serializeJson(different, text);
  Storage.put(exchangeFilePath(1), text);
  listed["sentinel"] = 42;
  CHECK(readSnapshotForBook(exchangeFilePath(1), book, listed) == ExchangeError::Invalid);
  CHECK(listed["sentinel"] == 42);
  Storage.put("/YomukaSync/broken.json", exported + "garbage");
  CHECK(readSnapshotForBook("/YomukaSync/broken.json", book, listed) == ExchangeError::Invalid);
  Storage.put("/YomukaSync/empty.json", "");
  CHECK(readSnapshotForBook("/YomukaSync/empty.json", book, listed) == ExchangeError::Invalid);
  Storage.put("/YomukaSync/large.json", std::string(65537, ' '));
  CHECK(readSnapshotForBook("/YomukaSync/large.json", book, listed) == ExchangeError::Invalid);
  CHECK(readSnapshotForBook("/YomukaSync/missing.json", book, listed) == ExchangeError::StorageFailure);
  CHECK(listed["sentinel"] == 42);
  const ExchangeBook small{1, 1, book.path, book.title, book.author};
  CHECK(readSnapshotForBook(renamed, small, listed) == ExchangeError::Invalid);
  snapshot["units"]["history"]["data"]["seconds"] = 1000;
  snapshot["units"]["history"]["data"]["sessionCount"] = 7;
  for (const char* unit : {"progress", "bookmarks", "readerSettings", "history"})
    snapshot["units"][unit]["updatedAt"] = 0;
  snapshot["units"]["readerSettings"]["data"].to<JsonObject>();
  const auto before = Storage.snapshot();
  std::vector<PreparedFile> prepared;
  CHECK(prepareSnapshotImport(book, snapshot, All, history, layout, nullptr, prepared) == ExchangeError::None);
  CHECK(Storage.snapshot() == before);
  CHECK(prepared.size() == 5);
  CHECK(applyPreparedFiles(prepared) == TransactionResult::Committed);
  CHECK(history.reloadAfterSync());
  Progress p;
  uint32_t date = 123;
  CHECK(readProgress(path, p, date) == ReadStatus::Present && date == 0 && p.chapterPage == 1 &&
        p.chapterPageCount == 5);
  std::vector<BookmarkEntry> bookmarks;
  CHECK(readBookmarks("/.crosspoint/books/0000000000000001/bookmarks.json", bookmarks, date) == ReadStatus::Present &&
        date == 0 && bookmarks[0].chapterPage == 1);
  CHECK(BookReaderSettings::readForSync(1, settings, date) == ReadStatus::Present &&
        !BookReaderSettings::hasAnyField(settings) && date == 0);
  CHECK(BookReaderSettings::readForSync(2, settings, date) == ReadStatus::Present && settings.writingMode == 2 &&
        date == 1710000000U);
  ReadingHistoryBook result;
  CHECK(history.readForSync(1, result, date) == ReadStatus::Present && result.seconds == 1000 &&
        result.sessionCount == 7 && date == 0);
  CHECK(history.getSummary().totalSeconds == 2);
  CHECK(prepareSnapshotImport(book, snapshot, History, history, nullptr, nullptr, prepared) == ExchangeError::None &&
        prepared.size() == 1);
  CHECK(applyPreparedFiles(prepared) == TransactionResult::Committed && history.reloadAfterSync());
  CHECK(history.readForSync(1, result, date) == ReadStatus::Present && result.seconds == 1000 &&
        result.sessionCount == 7);
  const auto storedProgress = Storage.snapshot().at(path);
  CHECK(prepareSnapshotImport(book, snapshot, Bookmarks, history, nullptr, nullptr, prepared) == ExchangeError::Layout);
  CHECK(Storage.snapshot().at(path) == storedProgress);
  // All setting names/enums/booleans and signed ruby offsets round-trip.
  BookReaderSettings::Override all;
  all.fields = 31;
  all.writingMode = 1;
  all.orientation = 3;
  all.bookStyle = 1;
  all.imageRendering = 2;
  all.invertImages = 1;
  all.horizontal.fields = all.vertical.fields = 4095;
  all.horizontal.values.fontFamily = 1;
  std::strcpy(all.horizontal.values.sdFontFamilyName, "JapaneseFont");
  all.horizontal.values.fontSize = 3;
  all.horizontal.values.lineSpacing = 220;
  all.horizontal.values.charSpacing = 50;
  all.horizontal.values.paragraphAlignment = 2;
  all.horizontal.values.extraParagraphSpacing = 4;
  all.horizontal.values.screenMargin = 40;
  all.horizontal.values.rubyOffsetX = 0;
  all.horizontal.values.rubyOffsetY = 80;
  all.horizontal.values.tateChuYokoMaxDigits = 3;
  all.vertical.values = all.horizontal.values;
  CHECK(BookReaderSettings::save(1, all));
  JsonDocument settingsSnapshot;
  CHECK(exportSnapshot(book, ReaderOverrides, history, settingsSnapshot) == ExchangeError::None);
  BookReaderSettings::Override decoded;
  CHECK(decodeSnapshotSettings(settingsSnapshot["units"]["readerSettings"]["data"].as<JsonObjectConst>(), decoded) ==
        ExchangeError::None);
  CHECK(decoded.fields == 31 && decoded.vertical.fields == 4095 && decoded.horizontal.fields == 4095);
  CHECK(decoded.orientation == 3 && decoded.writingMode == 1 && decoded.bookStyle == 1 && decoded.imageRendering == 2 &&
        decoded.invertImages == 1);
  CHECK(decoded.horizontal.values.rubyOffsetX == 0 && decoded.horizontal.values.rubyOffsetY == 80 &&
        decoded.horizontal.values.lineSpacing == 220);
  CHECK(std::strcmp(decoded.vertical.values.sdFontFamilyName, "JapaneseFont") == 0 &&
        decoded.vertical.values.fontSize == 3);
  // Two distinct source bookmarks may collapse onto one receiver page.
  auto duplicates = snapshot["units"]["bookmarks"]["data"].as<JsonArray>();
  auto extra = duplicates.add<JsonObject>();
  extra.set(duplicates[0]);
  extra["chapterPage"] = 3;
  CHECK(prepareSnapshotImport(book, snapshot, Bookmarks, history, layout, nullptr, prepared) == ExchangeError::Layout);
  Storage.put("/.crosspoint/reading-history.json",
              "{\"version\":1,\"totalSeconds\":5,\"books\":[{\"path\":\"/"
              "same.epub\",\"title\":\"Old\",\"author\":\"Author\",\"seconds\":5}],\"days\":[{\"date\":20241001,"
              "\"seconds\":5}]}");
  CHECK(prepareSnapshotImport(book, snapshot, History, history, nullptr, nullptr, prepared) == ExchangeError::None);
  CHECK(applyPreparedFiles(prepared) == TransactionResult::Committed && history.reloadAfterSync());
  CHECK(history.getBooks().size() == 1 && history.getBooks()[0].seconds == 1000 &&
        history.getBooks()[0].sessionCount == 7);
  CHECK(history.getSummary().totalSeconds == 5);
  snapshot["bookId"] = "0000000000000002";
  CHECK(prepareSnapshotImport(book, snapshot, All, history, layout, nullptr, prepared) == ExchangeError::Invalid);
  std::cout << "PASS SD exchange: " << checks << " checks\n";
}
