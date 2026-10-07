#pragma once
#include "BookReaderSettings.h"
#include "ImportTransaction.h"
#include "ReadingHistoryStore.h"
#include "SnapshotValidation.h"
#include "SyncProgress.h"
namespace yomuka::sync {
enum UnitMask : uint8_t { Position = 1, Bookmarks = 2, ReaderOverrides = 4, History = 8, All = 15 };
struct ExchangeBook {
  uint64_t id;
  uint32_t spines;
  std::string path, title, author;
};
enum class ExchangeError : uint8_t { None, Invalid, StorageFailure, MissingPosition, Layout, Recovery };
using ProjectPosition = bool (*)(const Progress&, const BookReaderSettings::Override*, Progress&, void*);
ExchangeError exportSnapshot(const ExchangeBook& book, uint8_t selected, ReadingHistoryStore& history,
                             JsonDocument& output);
ExchangeError decodeSnapshotSettings(JsonObjectConst data, BookReaderSettings::Override& output);
// Read-only preparation except disposable section caches produced by layout.
// Caller closed sessions; selected settings were checked for font availability.
ExchangeError prepareSnapshotImport(const ExchangeBook& book, const JsonDocument& snapshot, uint8_t selected,
                                    ReadingHistoryStore& history, ProjectPosition project, void* context,
                                    std::vector<PreparedFile>& output);
std::string exchangeFilePath(uint64_t bookId);
// Bounded full validation for listing; the selected file is checked again at import.
// Failure leaves output unchanged. File names are not book identity.
ExchangeError readSnapshotForBook(const std::string& path, const ExchangeBook& book, JsonDocument& output);
// Stage a validated received snapshot for later device-side confirmation.
// This only replaces /YomukaSync/<BookId>.json, never reader data.
ExchangeError stageSnapshotForBook(const ExchangeBook& book, const JsonDocument& snapshot);
}  // namespace yomuka::sync
