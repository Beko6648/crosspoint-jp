#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace yomuka::sync {
struct PreparedFile {
  std::string path;
  std::vector<uint8_t> bytes;
};
enum class TransactionResult : uint8_t { Committed, CleanupPending, Rejected, RolledBack, RecoveryRequired };
// Internal prepared-storage API, never accepts paths from exchange JSON.
// Caller must validate snapshot/BookId/fonts, close reading sessions, and
// prepare receiver layout before calling. Reload in-memory stores afterwards.
TransactionResult applyPreparedFiles(const std::vector<PreparedFile>& files);
// Run before loading reading state at boot. False blocks normal reading;
// retain journal/backups and retry after resolving SD failures.
bool recoverImportTransaction();
}  // namespace yomuka::sync
