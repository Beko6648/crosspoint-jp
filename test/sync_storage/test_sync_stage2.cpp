#include <ArduinoJson.h>
#include <HalStorage.h>

#include <iostream>
#include <set>
#include <string>

#include "sync/ImportTransaction.h"
#include "sync/SnapshotValidation.h"
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
const std::string a = "/.crosspoint/books/0123456789abcdef/progress.bin";
const std::string b = "/.crosspoint/books/0123456789abcdef/bookmarks.json";
const std::string c = "/.crosspoint/book-reader-settings.json";
const std::string d = a + ".sync-time";
const std::string e = "/.crosspoint/reading-history.json";
bool oldState(const FakeStorage::Snapshot& s) {
  return s.at(a) == "old" && s.at(c) == "previous" && !s.count(b) && s.at(d) == "old-time" && s.at(e) == "old-history";
}
bool newState(const FakeStorage::Snapshot& s) {
  return s.at(a) == "new" && s.at(c) == "c" && s.count(b) && s.at(b) == "b" && s.at(d) == "new-time" &&
         s.at(e) == "new-history";
}
int main(int argc, char**) {
  if (argc > 1) {
    std::string line;
    while (std::getline(std::cin, line)) {
      JsonDocument doc;
      doc["unchanged"] = true;
      const auto status = parseSnapshot(reinterpret_cast<const uint8_t*>(line.data()), line.size(), doc);
      std::cout << static_cast<int>(status) << ":" << (status == SnapshotError::None || doc["unchanged"] == true)
                << "\n";
    }
    return 0;
  }
  std::vector<PreparedFile> files{{a, {'n', 'e', 'w'}},
                                  {b, {'b'}},
                                  {c, {'c'}},
                                  {d, {'n', 'e', 'w', '-', 't', 'i', 'm', 'e'}},
                                  {e, {'n', 'e', 'w', '-', 'h', 'i', 's', 't', 'o', 'r', 'y'}}};
  Storage.put(d, "old-time");
  Storage.put(e, "old-history");
  Storage.put(a, "old");
  Storage.put(c, "previous");
  Storage.put("/unrelated", "keep");
  const auto original = Storage.snapshot();
  Storage.recording = true;
  CHECK(applyPreparedFiles(files) == TransactionResult::Committed);
  const int successfulCalls = Storage.calls;
  const auto cuts = Storage.snapshots;
  Storage.recording = false;
  const auto complete = Storage.snapshot();
  CHECK(complete.at(a) == "new" && complete.at(b) == "b" && complete.at(c) == "c");
  for (const auto& cut : cuts) {
    Storage.restore(cut);
    Storage.failAt = 0;
    Storage.calls = 0;
    CHECK(recoverImportTransaction());
    const auto recovered = Storage.snapshot();
    const bool old = oldState(recovered);
    const bool updated = newState(recovered);
    CHECK(old || updated);
    CHECK(recovered.at("/unrelated") == "keep");
    CHECK(recoverImportTransaction());
  }
  for (int fault = 1; fault <= successfulCalls + 20; ++fault) {
    Storage.restore(original);
    Storage.calls = 0;
    Storage.failAt = fault;
    applyPreparedFiles(files);
    Storage.failAt = 0;
    CHECK(recoverImportTransaction());
    const auto recovered = Storage.snapshot();
    CHECK(oldState(recovered) || newState(recovered));
  }
  // Interrupt every rollback mutation as well: repeated boot recovery must
  // converge without deleting a backup needed by another target.
  std::set<std::string> rollbackStates;
  for (const auto& cut : cuts) {
    if (!cut.count("/.crosspoint/sync-import.json") ||
        cut.at("/.crosspoint/sync-import.json").find("\"committed\":false") == std::string::npos)
      continue;
    // Journal temporary bytes do not alter rollback. Cover each distinct
    // target/final-backup state, and every mutation while restoring it.
    std::string state;
    for (const auto& path : {a, b, c, d, e}) {
      for (const auto& suffix : {std::string{}, std::string{".bak"}}) {
        const auto found = cut.find(path + suffix);
        state += found == cut.end() ? "<absent>|" : found->second + "|";
      }
    }
    if (!rollbackStates.insert(state).second) continue;
    Storage.restore(cut);
    Storage.snapshots.clear();
    Storage.recording = true;
    Storage.failAt = 0;
    CHECK(recoverImportTransaction());
    const auto rollbacks = Storage.snapshots;
    Storage.recording = false;
    for (const auto& interrupted : rollbacks) {
      Storage.restore(interrupted);
      CHECK(recoverImportTransaction());
      const auto recovered = Storage.snapshot();
      CHECK(oldState(recovered));
    }
  }
  Storage.restore(original);
  Storage.failRenameSource = a + ".tmp";
  CHECK(applyPreparedFiles(files) == TransactionResult::RecoveryRequired);
  Storage.failRenameSource.clear();
  CHECK(recoverImportTransaction());
  CHECK(oldState(Storage.snapshot()));
  Storage.restore(original);
  CHECK(applyPreparedFiles({{"/arbitrary", {'x'}}}) == TransactionResult::Rejected);
  CHECK(Storage.snapshot() == original);
  CHECK(applyPreparedFiles({{a, {'x'}}, {a, {'y'}}}) == TransactionResult::Rejected);
  CHECK(applyPreparedFiles({{a, {'x'}}, {"/.crosspoint/books/abcdef0123456789/progress.bin", {'y'}}}) ==
        TransactionResult::Rejected);
  CHECK(applyPreparedFiles({{a, {'x'}}}) == TransactionResult::Rejected);
  CHECK(applyPreparedFiles({{"/.crosspoint/books/0000000000000000/bookmarks.json", {'x'}}}) ==
        TransactionResult::Rejected);
  // A damaged backup must block rollback before touching any target.
  for (const auto& cut : cuts) {
    if (!cut.count("/.crosspoint/sync-import.json") ||
        cut.at("/.crosspoint/sync-import.json").find("\"committed\":false") == std::string::npos)
      continue;
    Storage.restore(cut);
    Storage.put("/.crosspoint/sync-import-0.old", "damaged");
    const auto damaged = Storage.snapshot();
    CHECK(!recoverImportTransaction());
    CHECK(Storage.snapshot() == damaged);
    break;
  }
  Storage.restore(original);
  Storage.put("/.crosspoint/sync-import.json", "{\"bad\":true}");
  CHECK(!recoverImportTransaction());
  CHECK(Storage.snapshot().at(a) == "old");
  JsonDocument snapshot;
  const std::string minimal =
      R"({"format":"yomuka-book-snapshot","formatVersion":1,"bookId":"0123456789abcdef","exportedAt":0,"units":{"progress":{"updatedAt":0,"data":{"spineIndex":1,"chapterPage":0,"chapterPageCount":0,"finished":true,"percent":100}}}})";
  CHECK(parseSnapshot(reinterpret_cast<const uint8_t*>(minimal.data()), minimal.size(), snapshot) ==
        SnapshotError::None);
  CHECK(validateSnapshotTarget(snapshot, "0123456789abcdef", 1) == SnapshotError::None);
  CHECK(validateSnapshotTarget(snapshot, "abcdef0123456789", 1) == SnapshotError::BookMismatch);
  snapshot["units"]["progress"]["data"]["finished"] = false;
  CHECK(validateSnapshotTarget(snapshot, "0123456789abcdef", 1) == SnapshotError::SpineRange);
  CHECK(snapshotSettingsAvailable(snapshot, nullptr, nullptr));
  snapshot["units"]["readerSettings"]["data"]["vertical"]["font"]["family"] = "noto-sans";
  snapshot["units"]["readerSettings"]["data"]["vertical"]["font"]["sdFamilyName"] = "missing";
  CHECK(!snapshotSettingsAvailable(snapshot, nullptr, nullptr));
  std::cout << "PASS stage2 transaction: " << checks << " checks; " << cuts.size() << " power-cut states\n";
}
