#include <HalStorage.h>

#include <cstdlib>
#include <ctime>
#include <iostream>

#include "BookReaderSettings.h"
#include "ReadingHistoryStore.h"
#include "sync/BookmarkStorage.h"
#include "sync/ProgressStorage.h"

using namespace yomuka::sync;
static size_t checks = 0;
#define CHECK(condition)                                \
  do {                                                  \
    ++checks;                                           \
    if (!(condition)) {                                 \
      std::cerr << __LINE__ << ": " #condition << '\n'; \
      std::exit(1);                                     \
    }                                                   \
  } while (false)
const std::string progressPath = "/.crosspoint/books/0000000000000001/progress.bin";
const std::string bookmarksPath = "/.crosspoint/books/0000000000000001/bookmarks.json";
const std::string settingsPath = "/.crosspoint/book-reader-settings.json";
const std::string historyPath = "/.crosspoint/reading-history.json";
const uint8_t firstProgress[] = {0, 0, 1, 0, 4, 0, 0, 25};
const uint8_t nextProgress[] = {0, 0, 2, 0, 4, 0, 0, 50};
void reset() {
  Storage = FakeStorage{};
  testUnixTime = 1710000000;
}
void reads() {
  reset();
  Progress progress;
  progress.spineIndex = 9;
  uint32_t date = 99;
  CHECK(readProgress(progressPath, progress, date) == ReadStatus::Absent);
  CHECK(progress.spineIndex == 9 && date == 99);
  Storage.online = false;
  CHECK(readProgress(progressPath, progress, date) == ReadStatus::StorageUnavailable);
  Storage.online = true;
  for (size_t length : {size_t(0), size_t(1), size_t(3), size_t(5), size_t(9)}) {
    Storage.put(progressPath, std::string(length, '\0'));
    CHECK(readProgress(progressPath, progress, date) == ReadStatus::Corrupt);
    CHECK(progress.spineIndex == 9 && date == 99);
  }
  for (size_t length : {size_t(4), size_t(6), size_t(7), size_t(8)}) {
    Storage.put(progressPath, std::string(reinterpret_cast<const char*>(firstProgress), length));
    CHECK(readProgress(progressPath, progress, date) == ReadStatus::Present);
    CHECK(date == 0 && progress.chapterPage == 1);
  }
  Storage.failRead = progressPath;
  CHECK(readProgress(progressPath, progress, date) == ReadStatus::IoError);
  JsonDocument document;
  for (const std::string& input : std::vector<std::string>{"", "{", "{} garbage", "{}{}", std::string("{}\0", 3)}) {
    Storage.put("json", input);
    CHECK(readJson("json", document) == ReadStatus::Corrupt);
  }
  Storage.put("json", "{} \r\n\t");
  CHECK(readJson("json", document) == ReadStatus::Present);
  CHECK(readJson("json", document, 1) == ReadStatus::LimitExceeded);
}
void progressTransactions() {
  reset();
  CHECK(saveProgress(progressPath, firstProgress, 8));
  const auto before = Storage.snapshot();
  testUnixTime = 1720000000;
  CHECK(saveProgress(progressPath, firstProgress, 8));
  CHECK(Storage.snapshot() == before);
  for (int failure = 1; failure <= 100; ++failure) {
    Storage = FakeStorage{};
    Storage.restore(before);
    Storage.failAt = failure;
    const bool saved = saveProgress(progressPath, nextProgress, 8);
    Storage.failAt = 0;
    Progress value;
    uint32_t timestamp = 0;
    CHECK(readProgress(progressPath, value, timestamp) == ReadStatus::Present);
    CHECK(value.chapterPage == (saved ? 2 : 1));
    CHECK(timestamp == (saved ? 1720000000U : 1710000000U));
  }
  Storage = FakeStorage{};
  Storage.restore(before);
  Storage.recording = true;
  CHECK(saveProgress(progressPath, nextProgress, 8));
  const auto snapshots = Storage.snapshots;
  CHECK(!snapshots.empty());
  for (const auto& snapshot : snapshots) {
    Storage = FakeStorage{};
    Storage.restore(snapshot);  // Reboot at every filesystem mutation.
    Progress value;
    uint32_t timestamp = 0;
    CHECK(readProgress(progressPath, value, timestamp) == ReadStatus::Present);
    CHECK((value.chapterPage == 1 && timestamp == 1710000000U) || (value.chapterPage == 2 && timestamp == 1720000000U));
  }
  testUnixTime = 0;
  CHECK(saveProgress(progressPath, firstProgress, 8));
  Progress value;
  uint32_t timestamp = 5;
  CHECK(readProgress(progressPath, value, timestamp) == ReadStatus::Present && timestamp == 0);
  reset();
  CHECK(saveProgress(progressPath, firstProgress, 8, false));
  CHECK(readProgress(progressPath, value, timestamp) == ReadStatus::Present && timestamp == 0);
}
void jsonTransactions() {
  reset();
  JsonDocument first, next;
  first["value"] = 1;
  first["updatedAt"] = 1710000000U;
  next["value"] = 2;
  next["updatedAt"] = 1720000000U;
  CHECK(writeJson("json", first));
  const auto before = Storage.snapshot();
  for (int failure = 1; failure <= 100; ++failure) {
    Storage = FakeStorage{};
    Storage.restore(before);
    Storage.failAt = failure;
    const bool saved = writeJson("json", next);
    Storage.failAt = 0;
    JsonDocument result;
    CHECK(readJson("json", result) == ReadStatus::Present);
    CHECK(result["value"].as<int>() == (saved ? 2 : 1));
    CHECK(result["updatedAt"].as<uint32_t>() == (saved ? 1720000000U : 1710000000U));
  }
  Storage = FakeStorage{};
  Storage.restore(before);
  Storage.recording = true;
  CHECK(writeJson("json", next));
  const auto snapshots = Storage.snapshots;
  for (const auto& snapshot : snapshots) {
    Storage = FakeStorage{};
    Storage.restore(snapshot);
    JsonDocument result;
    CHECK(readJson("json", result) == ReadStatus::Present);
    CHECK((result["value"].as<int>() == 1 && result["updatedAt"].as<uint32_t>() == 1710000000U) ||
          (result["value"].as<int>() == 2 && result["updatedAt"].as<uint32_t>() == 1720000000U));
  }
  Storage.failClose = "json.tmp";
  CHECK(!writeJson("json", first));
  JsonDocument result;
  CHECK(readJson("json", result) == ReadStatus::Present && result["value"].as<int>() == 2);
}
void bookmarks() {
  reset();
  BookmarkEntry bookmark;
  bookmark.summary = "test";
  bookmark.chapterPageCount = 4;
  bookmark.percentage = 0.12345678f;
  std::vector<BookmarkEntry> values{bookmark};
  CHECK(saveBookmarks(bookmarksPath, values));
  uint32_t date = 0;
  std::vector<BookmarkEntry> output;
  CHECK(readBookmarks(bookmarksPath, output, date) == ReadStatus::Present && date == 1710000000U);
  const auto before = Storage.snapshot();
  testUnixTime = 1720000000;
  CHECK(saveBookmarks(bookmarksPath, values));
  if (Storage.snapshot() != before)
    std::cerr << before.at(bookmarksPath) << "\n" << *Storage.files[bookmarksPath] << "\n";
  CHECK(Storage.snapshot() == before);
  Storage.failWrite = bookmarksPath + ".tmp";
  CHECK(!saveBookmarks(bookmarksPath, {}));
  CHECK(readBookmarks(bookmarksPath, output, date) == ReadStatus::Present && output.size() == 1 && date == 1710000000U);
  Storage.failWrite.clear();
  CHECK(saveBookmarks(bookmarksPath, {}));
  CHECK(readBookmarks(bookmarksPath, output, date) == ReadStatus::Present && output.empty() && date == 1720000000U);
  CHECK(!saveBookmarks(bookmarksPath, std::vector<BookmarkEntry>(25, bookmark)));
  Storage.put(bookmarksPath,
              "{\"bookmarks\":[{\"summary\":\"x\",\"percentage\":2,\"spine\":0,\"pages\":4,\"page\":0}]}");
  date = 17;
  CHECK(readBookmarks(bookmarksPath, output, date) == ReadStatus::Corrupt && date == 17);
  CHECK(!saveBookmarks(bookmarksPath, {}));
  for (float ratio : {0.0f, 0.1f, 1.0f, 0.3333333f, 0.000001f, 1.23456e-12f}) {
    reset();
    values[0].percentage = ratio;
    CHECK(saveBookmarks(bookmarksPath, values));
    const auto unchanged = Storage.snapshot();
    testUnixTime = 1720000000;
    CHECK(saveBookmarks(bookmarksPath, values) && Storage.snapshot() == unchanged);
  }
}
void settings() {
  reset();
  BookReaderSettings::Override value;
  value.fields = BookReaderSettings::WritingMode;
  value.writingMode = CrossPointSettings::WM_VERTICAL;
  CHECK(BookReaderSettings::save(1, value));
  CHECK(BookReaderSettings::save(2, value));
  BookReaderSettings::Override result;
  uint32_t date = 0;
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Present && date == 1710000000U);
  CHECK(result.fields == BookReaderSettings::WritingMode && result.horizontal.fields == 0);
  const auto before = Storage.snapshot();
  testUnixTime = 1720000000;
  CHECK(BookReaderSettings::save(1, value) && Storage.snapshot() == before);
  Storage.failWrite = settingsPath + ".tmp";
  CHECK(!BookReaderSettings::remove(1));
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Present && date == 1710000000U);
  Storage.failWrite.clear();
  CHECK(BookReaderSettings::remove(1));
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Present && date == 1720000000U);
  CHECK(!BookReaderSettings::hasAnyField(result));
  CHECK(BookReaderSettings::readForSync(2, result, date) == ReadStatus::Present && date == 1710000000U);
  Storage.put(settingsPath, "{\"formatVersion\":1,\"books\":{\"0000000000000001\":{\"horizontal\":42}}}");
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Corrupt);
  CHECK(!BookReaderSettings::save(1, value));
  Storage.put(settingsPath, "{\"formatVersion\":1,\"books\":{\"0000000000000001\":{\"writingMode\":2}}}");
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Present && date == 0);
  CHECK(BookReaderSettings::migrate(1, 2));
  CHECK(BookReaderSettings::readForSync(2, result, date) == ReadStatus::Present && date == 0);
  value.horizontal.fields = BookReaderSettings::DirectionFont;
  std::memset(value.horizontal.values.sdFontFamilyName, 'x', sizeof(value.horizontal.values.sdFontFamilyName));
  CHECK(!BookReaderSettings::save(2, value));
  Storage.put(settingsPath,
              "{\"formatVersion\":1,\"books\":{\"0000000000000001\":{\"horizontal\":{\"fontFamily\":null,"
              "\"sdFontFamilyName\":null}}}}");
  CHECK(BookReaderSettings::readForSync(1, result, date) == ReadStatus::Corrupt);
  testUnixTime = 0;
  Storage.put(settingsPath, "{\"formatVersion\":1,\"books\":{\"0000000000000002\":{\"writingMode\":2}}}");
  CHECK(BookReaderSettings::remove(2));
  CHECK(BookReaderSettings::readForSync(2, result, date) == ReadStatus::Present && date == 0);
}
void history() {
  reset();
  ReadingHistoryStore store;
  store.beginSession("a.epub", "A", "Author", 1);
  CHECK(store.flushPending());
  ReadingHistoryBook value;
  uint32_t date = 0;
  CHECK(store.readForSync(1, value, date) == ReadStatus::Present && value.sessionCount == 1 && date == 1710000000U);
  Storage.nowMs = 5000;
  testUnixTime = 1720000000;
  Storage.failWrite = historyPath + ".tmp";
  CHECK(!store.flushPending());
  CHECK(store.readForSync(1, value, date) == ReadStatus::Present && value.seconds == 0 && date == 1710000000U);
  CHECK(store.getBooks()[0].updatedAt == 1710000000U);
  Storage.failWrite.clear();
  testUnixTime = 1730000000;
  CHECK(store.flushPending());
  CHECK(store.readForSync(1, value, date) == ReadStatus::Present && value.seconds == 5 && date == 1720000000U);
  CHECK(store.endSession());
  CHECK(store.removeBook("a.epub", 1));
  CHECK(store.readForSync(1, value, date) == ReadStatus::Present && value.seconds == 0 && date == 1730000000U);
  CHECK(store.getSummary().totalSeconds == 5);
  ReadingHistoryStore reloaded;
  CHECK(reloaded.readForSync(1, value, date) == ReadStatus::Present && date == 1730000000U);
  Storage.put(historyPath, "{\"version\":2,\"totalSeconds\":0,\"books\":[{}],\"days\":[]}");
  ReadingHistoryStore corrupted;
  CHECK(corrupted.readForSync(1, value, date) == ReadStatus::Corrupt);
  corrupted.beginSession("b.epub", "B", "", 2);
  CHECK(!corrupted.flushPending());
  CHECK(Storage.files[historyPath]->find("[{}]") != std::string::npos);
  reset();
  ReadingHistoryStore deleted;
  deleted.beginSession("a.epub", "A", "", 1);
  CHECK(deleted.endSession());
  deleted.beginSession("b.epub", "B", "", 2);
  CHECK(deleted.endSession());
  testUnixTime = 1720000000;
  CHECK(deleted.removeBook("a.epub", 1));
  CHECK(deleted.readForSync(2, value, date) == ReadStatus::Present && date == 1710000000U);
  Storage.failWrite = historyPath + ".tmp";
  CHECK(!deleted.clearAll());
  CHECK(deleted.readForSync(2, value, date) == ReadStatus::Present && value.sessionCount == 1);
  Storage.failWrite.clear();
  CHECK(deleted.clearAll());
  CHECK(deleted.readForSync(2, value, date) == ReadStatus::Present && value.sessionCount == 0 && date == 1720000000U);
  testUnixTime = 0;
  deleted.beginSession("b.epub", "B", "", 2);
  CHECK(deleted.endSession());
  CHECK(deleted.readForSync(2, value, date) == ReadStatus::Present && value.sessionCount == 1 && date == 0);
}
int main() {
  reads();
  progressTransactions();
  jsonTransactions();
  bookmarks();
  settings();
  history();
  std::cout << "PASS sync storage: " << checks << " checks\n";
}
