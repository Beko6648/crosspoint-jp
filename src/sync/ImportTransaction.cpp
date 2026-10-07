#include "ImportTransaction.h"

#include <ArduinoJson.h>
#include <HalStorage.h>

#include <cstring>

#include "StorageIo.h"

namespace yomuka::sync {
namespace {
constexpr const char* journal = "/.crosspoint/sync-import.json";
constexpr size_t limit = 262144;
std::string backup(size_t index) { return "/.crosspoint/sync-import-" + std::to_string(index) + ".old"; }
bool allowed(const std::string& path) {
  if (path == "/.crosspoint/book-reader-settings.json" || path == "/.crosspoint/reading-history.json") return true;
  const std::string prefix = "/.crosspoint/books/";
  if (path.compare(0, prefix.size(), prefix) != 0 || path.size() < prefix.size() + 17) return false;
  for (size_t i = prefix.size(); i < prefix.size() + 16; ++i)
    if (!((path[i] >= '0' && path[i] <= '9') || (path[i] >= 'a' && path[i] <= 'f'))) return false;
  const std::string suffix = path.substr(prefix.size() + 16);
  if (path.substr(prefix.size(), 16) == "0000000000000000") return false;
  return suffix == "/progress.bin" || suffix == "/progress.bin.sync-time" || suffix == "/bookmarks.json";
}
ReadStatus inspect(const std::string& path, size_t& length, uint64_t& hash) {
  if (!Storage.ready()) return ReadStatus::StorageUnavailable;
  if (!recoverFile(path)) return ReadStatus::IoError;
  if (!Storage.exists(path.c_str())) {
    length = 0;
    hash = 0;
    return ReadStatus::Absent;
  }
  HalFile file;
  if (!Storage.openFileForRead("SYNC", path, file)) return ReadStatus::IoError;
  const size_t size = file.size();
  if (size > limit) return ReadStatus::LimitExceeded;
  uint64_t value = 14695981039346656037ULL;
  uint8_t bytes[256];
  size_t remaining = size;
  while (remaining) {
    const size_t count = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
    if (file.read(bytes, count) != static_cast<int>(count)) return ReadStatus::IoError;
    for (size_t i = 0; i < count; ++i) value = (value ^ bytes[i]) * 1099511628211ULL;
    remaining -= count;
  }
  if (!file.close()) return ReadStatus::IoError;
  length = size;
  hash = value;
  return ReadStatus::Present;
}
bool erase(const std::string& path) { return !Storage.exists(path.c_str()) || Storage.remove(path.c_str()); }
bool eraseAll(const std::string& path) { return erase(path) && erase(path + ".bak") && erase(path + ".tmp"); }
bool validJournal(JsonDocument& document) {
  if (!document.is<JsonObject>() || document.size() != 3 || document["version"] != 1 ||
      !document["version"].is<uint8_t>() || !document["committed"].is<bool>() || !document["files"].is<JsonArray>())
    return false;
  const auto files = document["files"].as<JsonArrayConst>();
  if (files.size() == 0 || files.size() > 5) return false;
  std::string book;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto item = files[i];
    if (!item.is<JsonObjectConst>() || item.size() != 4 || !item["path"].is<const char*>() ||
        !item["existed"].is<bool>() || !item["size"].is<uint32_t>() || !item["hash"].is<uint64_t>() ||
        item["size"].as<uint32_t>() > limit)
      return false;
    const std::string path = item["path"].as<std::string>();
    if (!allowed(path)) return false;
    for (size_t j = 0; j < i; ++j)
      if (path == files[j]["path"].as<std::string>()) return false;
    const std::string prefix = "/.crosspoint/books/";
    if (path.compare(0, prefix.size(), prefix) == 0) {
      const auto id = path.substr(prefix.size(), 16);
      if (!book.empty() && id != book) return false;
      book = id;
    }
    const bool position = path.size() >= 13 && path.substr(path.size() - 13) == "/progress.bin";
    const bool timestamp = path.size() >= 10 && path.substr(path.size() - 10) == ".sync-time";
    if (position || timestamp) {
      const std::string companion = position ? path + ".sync-time" : path.substr(0, path.size() - 10);
      bool found = false;
      for (JsonVariantConst other : files)
        if (other["path"] == companion) found = true;
      if (!found) return false;
    }
  }
  return true;
}
bool cleanup(size_t count) {
  // Remove the recovery decision first only for a committed transaction.
  // Orphan backups are harmless and overwritten before the next transaction.
  // Never leave an older .bak decision after removing the final journal.
  if (!erase(std::string(journal) + ".bak") || !erase(std::string(journal) + ".tmp") || !erase(journal)) return false;
  for (size_t i = 0; i < count; ++i)
    if (!eraseAll(backup(i))) return false;
  return true;
}
}  // namespace
bool recoverImportTransaction() {
  HalStorage::StorageLock lock;
  if (!Storage.ready()) return false;
  JsonDocument document;
  const auto status = readJson(journal, document, 4096);
  if (status == ReadStatus::Absent) return true;
  if (status != ReadStatus::Present || !validJournal(document)) return false;
  const auto files = document["files"].as<JsonArrayConst>();
  if (!document["committed"].as<bool>()) {
    // Verify every backup before changing any target. Do not delete backups
    // until all rollback writes and the durable recovery decision complete.
    size_t length = 0;
    uint64_t hash = 0;
    for (size_t i = 0; i < files.size(); ++i) {
      if (!files[i]["existed"].as<bool>()) continue;
      if (inspect(backup(i), length, hash) != ReadStatus::Present || length != files[i]["size"].as<uint32_t>() ||
          hash != files[i]["hash"].as<uint64_t>())
        return false;
    }
    for (size_t i = 0; i < files.size(); ++i) {
      const auto path = files[i]["path"].as<std::string>();
      if (files[i]["existed"].as<bool>()) {
        if (!copyStoredFile(backup(i), path)) return false;
      } else if (!eraseAll(path))
        return false;
    }
    // Once rollback is complete, a committed flag means cleanup-only too.
    document["committed"] = true;
    if (!writeJson(journal, document)) return false;
  }
  return cleanup(files.size());
}
TransactionResult applyPreparedFiles(const std::vector<PreparedFile>& files) {
  HalStorage::StorageLock lock;
  if (files.empty() || files.size() > 5) return TransactionResult::Rejected;
  JsonDocument document;
  document["version"] = 1;
  document["committed"] = false;
  auto entries = document["files"].to<JsonArray>();
  for (const auto& file : files) {
    if (file.bytes.size() > limit) return TransactionResult::Rejected;
    auto item = entries.add<JsonObject>();
    item["path"] = file.path;
    item["existed"] = false;
    item["size"] = 0;
    item["hash"] = uint64_t{0};
  }
  if (document.overflowed() || !validJournal(document)) return TransactionResult::Rejected;
  if (!recoverImportTransaction()) return TransactionResult::RecoveryRequired;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return TransactionResult::Rejected;
  size_t length = 0;
  uint64_t hash = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto status = inspect(files[i].path, length, hash);
    if (status != ReadStatus::Present && status != ReadStatus::Absent) return TransactionResult::Rejected;
    entries[i]["existed"] = status == ReadStatus::Present;
    entries[i]["size"] = length;
    entries[i]["hash"] = hash;
    if (status == ReadStatus::Present && !copyStoredFile(files[i].path, backup(i))) return TransactionResult::Rejected;
  }
  if (!writeJson(journal, document)) return TransactionResult::Rejected;
  for (const auto& file : files) {
    if (!writeBytes(file.path, file.bytes.data(), file.bytes.size()))
      return recoverImportTransaction() ? TransactionResult::RolledBack : TransactionResult::RecoveryRequired;
  }
  document["committed"] = true;
  if (!writeJson(journal, document))
    return recoverImportTransaction() ? TransactionResult::RolledBack : TransactionResult::RecoveryRequired;
  return cleanup(files.size()) ? TransactionResult::Committed : TransactionResult::CleanupPending;
}
}  // namespace yomuka::sync
