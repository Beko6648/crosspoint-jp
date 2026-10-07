#pragma once

#include <cstdint>
#include <limits>

namespace yomuka {
namespace sync {

struct ClockReading {
  bool known = false;
  std::uint64_t unixSeconds = 0;
};

enum class LocalSaveOutcome : std::uint8_t { Unchanged, Failed, Saved };

// Pure policy, NOT a persistence API. The data and its chosen timestamp must
// be committed/recovered together by the future storage adapter. Calling this
// after one independent write does not make two files atomic. "Saved" means
// an actual changed local value, not export, inspection or replaying an import.
constexpr std::uint32_t timestampAfterLocalSave(const std::uint32_t previous, const LocalSaveOutcome outcome,
                                                const ClockReading clock) noexcept {
  if (outcome != LocalSaveOutcome::Saved) return previous;
  if (!clock.known || clock.unixSeconds == 0U || clock.unixSeconds > std::numeric_limits<std::uint32_t>::max()) {
    return 0U;
  }
  return static_cast<std::uint32_t>(clock.unixSeconds);
}

// Import keeps the received timestamp, including zero. No wall clock lookup.
constexpr std::uint32_t timestampForImport(const std::uint32_t received) noexcept { return received; }

enum class TimestampComparison : std::uint8_t { Unknown, Equal, LocalNewer, IncomingNewer };

// A UI hint ONLY. Every import still requires explicit user selection; even
// valid unequal timestamps do not establish that two device clocks agree.
constexpr TimestampComparison compareTimestamps(const std::uint32_t local, const std::uint32_t incoming) noexcept {
  if (local == 0U || incoming == 0U) return TimestampComparison::Unknown;
  if (local == incoming) return TimestampComparison::Equal;
  return local > incoming ? TimestampComparison::LocalNewer : TimestampComparison::IncomingNewer;
}

}  // namespace sync
}  // namespace yomuka
