#include "SnapshotExchange.h"

#include <HalStorage.h>

#include <cstdio>
#include <cstring>

#include "BookmarkStorage.h"
#include "ProgressStorage.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif
namespace yomuka::sync {
ExchangeError readSnapshotForBook(const std::string& path, const ExchangeBook& book, JsonDocument& output) {
  HalFile file;
  if (!Storage.openFileForRead("SYNC", path, file)) return ExchangeError::StorageFailure;
  const size_t size = file.size();
  if (!file.close()) return ExchangeError::StorageFailure;
  if (!size || size > 65536) return ExchangeError::Invalid;
#if defined(ARDUINO_ARCH_ESP32)
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < size + 4096 ||
      heap_caps_get_free_size(MALLOC_CAP_8BIT) < size * 3 + 32768)
    return ExchangeError::StorageFailure;
#endif
  std::vector<uint8_t> raw(size);
  size_t length = 0;
  if (readBytes(path, raw.data(), raw.size(), length) != ReadStatus::Present) return ExchangeError::StorageFailure;
  JsonDocument candidate;
  if (parseSnapshot(raw.data(), length, candidate) != SnapshotError::None) return ExchangeError::Invalid;
  char id[17];
  snprintf(id, sizeof(id), "%016llx", static_cast<unsigned long long>(book.id));
  if (validateSnapshotTarget(candidate, id, book.spines) != SnapshotError::None) return ExchangeError::Invalid;
  output = std::move(candidate);
  return ExchangeError::None;
}
namespace {
const char* writing[] = {"auto", "horizontal", "vertical"};
const char* orientations[] = {"portrait", "landscape-cw", "inverted", "landscape-ccw"};
const char* styles[] = {"crosspoint", "epub", "balanced"};
const char* images[] = {"display", "placeholder", "suppress"};
const char* families[] = {"noto-serif", "noto-sans", "open-dyslexic"};
const char* sizes[] = {"small", "medium", "large", "extra-large"};
const char* alignments[] = {"justified", "left", "center", "right"};
std::string key(uint64_t id) {
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(id));
  return buffer;
}
std::string base(uint64_t id) { return "/.crosspoint/books/" + key(id); }
bool usable(ReadStatus status) { return status == ReadStatus::Present || status == ReadStatus::Absent; }
uint8_t enumValue(JsonVariantConst v, const char* const* names, size_t count) {
  for (size_t i = 0; i < count; ++i)
    if (v == names[i]) return i;
  return 255;
}
void encodeDirection(JsonObject o, const BookReaderSettings::DirectionOverride& d) {
  using namespace BookReaderSettings;
  const auto& v = d.values;
  if (d.fields & DirectionFont) {
    o["font"]["family"] = families[v.fontFamily];
    o["font"]["sdFamilyName"] = std::string(v.sdFontFamilyName);
  }
  if (d.fields & DirectionFontSize) o["fontSize"] = sizes[v.fontSize];
  if (d.fields & DirectionAlignment) o["paragraphAlignment"] = alignments[v.paragraphAlignment];
#define EXPORT_FIELD(bit, field) \
  if (d.fields & bit) o[#field] = v.field
  EXPORT_FIELD(DirectionLineSpacing, lineSpacing);
  EXPORT_FIELD(DirectionCharSpacing, charSpacing);
  EXPORT_FIELD(DirectionParagraphSpacing, extraParagraphSpacing);
  EXPORT_FIELD(DirectionMargin, screenMargin);
  EXPORT_FIELD(DirectionTateChuYokoDigits, tateChuYokoMaxDigits);
#undef EXPORT_FIELD
  if (d.fields & DirectionIndent) o["firstLineIndent"] = bool(v.firstLineIndent);
  if (d.fields & DirectionRubyEnabled) o["rubyEnabled"] = bool(v.rubyEnabled);
  if (d.fields & DirectionRubyOffsetX) o["rubyOffsetX"] = int(v.rubyOffsetX) - 16;
  if (d.fields & DirectionRubyOffsetY) o["rubyOffsetY"] = int(v.rubyOffsetY) - 16;
}
void decodeDirection(JsonObjectConst o, BookReaderSettings::DirectionOverride& d) {
  using namespace BookReaderSettings;
  auto& v = d.values;
  if (!o["font"].isUnbound()) {
    d.fields |= DirectionFont;
    v.fontFamily = enumValue(o["font"]["family"], families, 3);
    const auto s = o["font"]["sdFamilyName"].as<JsonString>();
    std::memcpy(v.sdFontFamilyName, s.c_str(), s.size());
    v.sdFontFamilyName[s.size()] = 0;
  }
  if (!o["fontSize"].isUnbound()) {
    d.fields |= DirectionFontSize;
    v.fontSize = enumValue(o["fontSize"], sizes, 4);
  }
  if (!o["paragraphAlignment"].isUnbound()) {
    d.fields |= DirectionAlignment;
    v.paragraphAlignment = enumValue(o["paragraphAlignment"], alignments, 4);
  }
#define IMPORT_FIELD(bit, field)       \
  if (!o[#field].isUnbound()) {        \
    d.fields |= bit;                   \
    v.field = o[#field].as<uint8_t>(); \
  }
  IMPORT_FIELD(DirectionLineSpacing, lineSpacing);
  IMPORT_FIELD(DirectionCharSpacing, charSpacing);
  IMPORT_FIELD(DirectionParagraphSpacing, extraParagraphSpacing);
  IMPORT_FIELD(DirectionMargin, screenMargin);
  IMPORT_FIELD(DirectionTateChuYokoDigits, tateChuYokoMaxDigits);
#undef IMPORT_FIELD
  if (!o["firstLineIndent"].isUnbound()) {
    d.fields |= DirectionIndent;
    v.firstLineIndent = o["firstLineIndent"].as<bool>();
  }
  if (!o["rubyEnabled"].isUnbound()) {
    d.fields |= DirectionRubyEnabled;
    v.rubyEnabled = o["rubyEnabled"].as<bool>();
  }
  if (!o["rubyOffsetX"].isUnbound()) {
    d.fields |= DirectionRubyOffsetX;
    v.rubyOffsetX = o["rubyOffsetX"].as<int>() + 16;
  }
  if (!o["rubyOffsetY"].isUnbound()) {
    d.fields |= DirectionRubyOffsetY;
    v.rubyOffsetY = o["rubyOffsetY"].as<int>() + 16;
  }
}
void encodeSettings(JsonObject o, const BookReaderSettings::Override& v) {
  using namespace BookReaderSettings;
  if (v.horizontal.fields) encodeDirection(o["horizontal"].to<JsonObject>(), v.horizontal);
  if (v.vertical.fields) encodeDirection(o["vertical"].to<JsonObject>(), v.vertical);
  if (v.fields & WritingMode) o["writingMode"] = writing[v.writingMode];
  if (v.fields & Orientation) o["orientation"] = orientations[v.orientation];
  if (v.fields & BookStyle) o["bookStyle"] = styles[v.bookStyle];
  if (v.fields & ImageRendering) o["imageRendering"] = images[v.imageRendering];
  if (v.fields & InvertImages) o["invertImages"] = bool(v.invertImages);
}
Progress decodeProgress(JsonVariantConst p) {
  Progress v;
  v.spineIndex = p["spineIndex"];
  v.chapterPage = p["chapterPage"];
  if (!p["chapterPageCount"].isNull()) v.chapterPageCount = p["chapterPageCount"].as<uint16_t>();
  if (!p["finished"].isNull()) v.finished = p["finished"].as<bool>();
  if (!p["percent"].isNull()) v.percent = p["percent"].as<uint8_t>();
  return v;
}
bool jsonFile(const std::string& path, const JsonDocument& doc, std::vector<PreparedFile>& files) {
  const auto size = measureJson(doc);
  if (doc.overflowed() || size > 262144) return false;
#if defined(ARDUINO_ARCH_ESP32)
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < size + 256 ||
      heap_caps_get_free_size(MALLOC_CAP_8BIT) < size + 12288)
    return false;
#endif
  PreparedFile file{path, std::vector<uint8_t>(size + 1)};
  if (serializeJson(doc, reinterpret_cast<char*>(file.bytes.data()), file.bytes.size()) != size) return false;
  file.bytes.resize(size);
  files.push_back(std::move(file));
  return true;
}
}  // namespace
std::string exchangeFilePath(uint64_t bookId) { return "/YomukaSync/" + key(bookId) + ".json"; }
ExchangeError decodeSnapshotSettings(JsonObjectConst data, BookReaderSettings::Override& output) {
  // Validate this unit through the same frozen schema before enum/index use.
  JsonDocument wrapper;
  wrapper["format"] = "yomuka-book-snapshot";
  wrapper["formatVersion"] = 1;
  wrapper["bookId"] = "0000000000000001";
  wrapper["exportedAt"] = 0;
  wrapper["units"]["readerSettings"]["updatedAt"] = 0;
  wrapper["units"]["readerSettings"]["data"].set(data);
  if (validateSnapshotDocument(wrapper) != SnapshotError::None) return ExchangeError::Invalid;
  BookReaderSettings::Override value;
  decodeDirection(data["horizontal"].as<JsonObjectConst>(), value.horizontal);
  decodeDirection(data["vertical"].as<JsonObjectConst>(), value.vertical);
  using namespace BookReaderSettings;
  if (!data["writingMode"].isUnbound()) {
    value.fields |= WritingMode;
    value.writingMode = enumValue(data["writingMode"], writing, 3);
  }
  if (!data["orientation"].isUnbound()) {
    value.fields |= Orientation;
    value.orientation = enumValue(data["orientation"], orientations, 4);
  }
  if (!data["bookStyle"].isUnbound()) {
    value.fields |= BookStyle;
    value.bookStyle = enumValue(data["bookStyle"], styles, 3);
  }
  if (!data["imageRendering"].isUnbound()) {
    value.fields |= ImageRendering;
    value.imageRendering = enumValue(data["imageRendering"], images, 3);
  }
  if (!data["invertImages"].isUnbound()) {
    value.fields |= InvertImages;
    value.invertImages = data["invertImages"].as<bool>();
  }
  output = value;
  return ExchangeError::None;
}
ExchangeError exportSnapshot(const ExchangeBook& book, uint8_t selected, ReadingHistoryStore& history,
                             JsonDocument& output) {
  HalStorage::StorageLock lock;
  if (!book.id || !selected || (selected & ~All) || !history.flushPending()) return ExchangeError::StorageFailure;
  JsonDocument doc;
  doc["format"] = "yomuka-book-snapshot";
  doc["formatVersion"] = 1;
  doc["bookId"] = key(book.id);
  doc["exportedAt"] = localUpdateTime();
  auto units = doc["units"].to<JsonObject>();
  uint32_t date = 0;
  if (selected & Position) {
    Progress v;
    const auto status = readProgress(base(book.id) + "/progress.bin", v, date);
    if (status != ReadStatus::Present)
      return status == ReadStatus::Absent ? ExchangeError::MissingPosition : ExchangeError::StorageFailure;
    auto u = units["progress"].to<JsonObject>();
    u["updatedAt"] = date;
    auto p = u["data"].to<JsonObject>();
    p["spineIndex"] = v.spineIndex;
    p["chapterPage"] = v.chapterPage;
    if (v.chapterPageCount)
      p["chapterPageCount"] = *v.chapterPageCount;
    else
      p["chapterPageCount"] = nullptr;
    if (v.finished)
      p["finished"] = *v.finished;
    else
      p["finished"] = nullptr;
    if (v.percent)
      p["percent"] = *v.percent;
    else
      p["percent"] = nullptr;
  }
  if (selected & Bookmarks) {
    std::vector<BookmarkEntry> values;
    date = 0;
    if (!usable(readBookmarks(base(book.id) + "/bookmarks.json", values, date))) return ExchangeError::StorageFailure;
    auto u = units["bookmarks"].to<JsonObject>();
    u["updatedAt"] = date;
    auto entries = u["data"].to<JsonArray>();
    for (const auto& v : values) {
      auto o = entries.add<JsonObject>();
      o["summary"] = v.summary;
      o["progressRatio"] = v.percentage;
      o["spineIndex"] = v.spineIndex;
      o["chapterPageCount"] = v.chapterPageCount;
      o["chapterPage"] = v.chapterPage;
    }
  }
  if (selected & ReaderOverrides) {
    BookReaderSettings::Override v;
    date = 0;
    if (!usable(BookReaderSettings::readForSync(book.id, v, date))) return ExchangeError::StorageFailure;
    units["readerSettings"]["updatedAt"] = date;
    encodeSettings(units["readerSettings"]["data"].to<JsonObject>(), v);
  }
  if (selected & History) {
    ReadingHistoryBook v;
    date = 0;
    if (!usable(history.readForSync(book.id, v, date))) return ExchangeError::StorageFailure;
    auto u = units["history"].to<JsonObject>();
    u["updatedAt"] = date;
    auto o = u["data"].to<JsonObject>();
    o["seconds"] = v.seconds;
    o["sessionCount"] = v.sessionCount;
    o["lastReadAt"] = v.lastReadAt;
    o["finishedAt"] = v.finishedAt;
    o["finished"] = v.finished;
  }
  if (validateSnapshotDocument(doc) != SnapshotError::None ||
      validateSnapshotTarget(doc, key(book.id).c_str(), book.spines) != SnapshotError::None)
    return ExchangeError::Invalid;
  output = std::move(doc);
  return ExchangeError::None;
}
ExchangeError prepareSnapshotImport(const ExchangeBook& book, const JsonDocument& snapshot, uint8_t selected,
                                    ReadingHistoryStore& history, ProjectPosition project, void* context,
                                    std::vector<PreparedFile>& output) {
  // Defensive validation even if caller has already built a preview.
  if (!selected || (selected & ~All) || validateSnapshotDocument(snapshot) != SnapshotError::None ||
      validateSnapshotTarget(snapshot, key(book.id).c_str(), book.spines) != SnapshotError::None)
    return ExchangeError::Invalid;
  auto units = snapshot["units"].as<JsonObjectConst>();
  const char* names[] = {"progress", "bookmarks", "readerSettings", "history"};
  for (unsigned i = 0; i < 4; ++i)
    if ((selected & (1 << i)) && units[names[i]].isUnbound()) return ExchangeError::Invalid;
  BookReaderSettings::Override settings;
  const BookReaderSettings::Override* layoutSettings = nullptr;
  if (selected & ReaderOverrides) {
    if (decodeSnapshotSettings(units["readerSettings"]["data"].as<JsonObjectConst>(), settings) != ExchangeError::None)
      return ExchangeError::Invalid;
    layoutSettings = &settings;
  }
  std::vector<PreparedFile> files;
  if (selected & Position) {
    Progress target;
    if (!project || !project(decodeProgress(units["progress"]["data"]), layoutSettings, target, context))
      return ExchangeError::Layout;
    if (validateProgressForBook(target, book.spines) != ProgressError::None) return ExchangeError::Layout;
    uint8_t raw[8];
    size_t length = 0;
    if (encodeLegacyProgress(target, raw, sizeof(raw), length) != ProgressError::None) return ExchangeError::Layout;
    const auto path = base(book.id) + "/progress.bin";
    std::vector<uint8_t> metadata;
    if (!prepareProgressTime(path, raw, length, units["progress"]["updatedAt"], metadata))
      return ExchangeError::StorageFailure;
    files.push_back({path, std::vector<uint8_t>(raw, raw + length)});
    files.push_back({path + ".sync-time", std::move(metadata)});
  }
  if (selected & Bookmarks) {
    JsonDocument doc;
    doc["updatedAt"] = units["bookmarks"]["updatedAt"];
    auto entries = doc["bookmarks"].to<JsonArray>();
    for (JsonVariantConst b : units["bookmarks"]["data"].as<JsonArrayConst>()) {
      Progress source;
      source.spineIndex = b["spineIndex"];
      source.chapterPage = b["chapterPage"];
      source.chapterPageCount = b["chapterPageCount"].as<uint16_t>();
      Progress target;
      if (!project || !project(source, layoutSettings, target, context) || !target.chapterPageCount ||
          *target.chapterPageCount == 0)
        return ExchangeError::Layout;
      if (validateProgressForBook(target, book.spines) != ProgressError::None) return ExchangeError::Layout;
      for (JsonVariantConst old : entries)
        if (old["spine"] == target.spineIndex && old["page"] == target.chapterPage) return ExchangeError::Layout;
      auto o = entries.add<JsonObject>();
      o["summary"] = b["summary"];
      o["percentage"] = b["progressRatio"];
      o["spine"] = target.spineIndex;
      o["page"] = target.chapterPage;
      o["pages"] = *target.chapterPageCount;
    }
    if (!jsonFile(base(book.id) + "/bookmarks.json", doc, files)) return ExchangeError::StorageFailure;
  }
  if (selected & ReaderOverrides) {
    JsonDocument doc;
    if (!BookReaderSettings::prepareForSync(book.id, settings, units["readerSettings"]["updatedAt"], doc) ||
        !jsonFile("/.crosspoint/book-reader-settings.json", doc, files))
      return ExchangeError::StorageFailure;
  }
  if (selected & History) {
    ReadingHistoryBook value;
    auto data = units["history"]["data"];
    value.bookId = book.id;
    value.path = book.path;
    value.title = book.title;
    value.author = book.author;
    value.seconds = data["seconds"];
    value.sessionCount = data["sessionCount"];
    value.lastReadAt = data["lastReadAt"];
    value.finishedAt = data["finishedAt"];
    value.finished = data["finished"];
    JsonDocument doc;
    if (!history.prepareForSync(value, units["history"]["updatedAt"], doc) ||
        !jsonFile("/.crosspoint/reading-history.json", doc, files))
      return ExchangeError::StorageFailure;
  }
  output = std::move(files);
  return ExchangeError::None;
}
}  // namespace yomuka::sync
