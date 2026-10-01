#include "SyncProgress.h"

#include <cstring>

namespace yomuka {
namespace sync {
namespace {
std::uint16_t readLe16(const std::uint8_t* bytes) noexcept {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
                                    (static_cast<std::uint16_t>(bytes[1]) << 8U));
}

void writeLe16(std::uint8_t* bytes, std::uint16_t value) noexcept {
  bytes[0] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[1] = static_cast<std::uint8_t>(value >> 8U);
}
}  // namespace

const char* progressErrorName(const ProgressError error) noexcept {
  switch (error) {
    case ProgressError::None:
      return "ok";
    case ProgressError::UnsupportedLength:
      return "unsupported-length";
    case ProgressError::NullBuffer:
      return "null-buffer";
    case ProgressError::InvalidFinishedFlag:
      return "invalid-finished-flag";
    case ProgressError::InvalidPercent:
      return "invalid-percent";
    case ProgressError::InvalidPageRange:
      return "invalid-page-range";
    case ProgressError::InvalidSpineCount:
      return "invalid-spine-count";
    case ProgressError::SpineOutOfRange:
      return "spine-out-of-range";
    case ProgressError::InvalidTerminal:
      return "invalid-terminal";
    case ProgressError::LayoutNotReady:
      return "layout-not-ready";
    case ProgressError::UnrepresentableLegacyState:
      return "unrepresentable-legacy-state";
    case ProgressError::BufferTooSmall:
      return "buffer-too-small";
  }
  return "unknown-error";
}

ProgressError validateProgressShape(const Progress& value) noexcept {
  if (value.percent && *value.percent > 100U) return ProgressError::InvalidPercent;
  if (value.chapterPageCount) {
    const auto count = *value.chapterPageCount;
    if ((count == 0U && value.chapterPage != 0U) || (count != 0U && value.chapterPage >= count)) {
      return ProgressError::InvalidPageRange;
    }
  }
  return ProgressError::None;
}

ProgressError decodeLegacyProgress(const std::uint8_t* bytes, const std::size_t size, Progress& out) noexcept {
  if (size != 4U && size != 6U && size != 7U && size != 8U) return ProgressError::UnsupportedLength;
  if (bytes == nullptr) return ProgressError::NullBuffer;

  Progress candidate;
  candidate.spineIndex = readLe16(bytes);
  candidate.chapterPage = readLe16(bytes + 2);
  if (size >= 6U) candidate.chapterPageCount = readLe16(bytes + 4);
  if (size >= 7U) {
    if (bytes[6] > 1U) return ProgressError::InvalidFinishedFlag;
    candidate.finished = bytes[6] != 0U;
  }
  if (size == 8U && bytes[7] != 255U) {
    if (bytes[7] > 100U) return ProgressError::InvalidPercent;
    candidate.percent = bytes[7];
  }
  const auto error = validateProgressShape(candidate);
  if (error != ProgressError::None) return error;
  out = candidate;
  return ProgressError::None;
}

ProgressError validateProgressForBook(const Progress& value, const std::uint32_t spineCount) noexcept {
  const auto error = validateProgressShape(value);
  if (error != ProgressError::None) return error;
  if (spineCount == 0U) return ProgressError::InvalidSpineCount;
  const auto index = static_cast<std::uint32_t>(value.spineIndex);
  if (index < spineCount) return ProgressError::None;
  if (index > spineCount) return ProgressError::SpineOutOfRange;
  if (value.chapterPage != 0U || !value.chapterPageCount || *value.chapterPageCount != 0U || !value.finished ||
      !*value.finished || !value.percent || *value.percent != 100U) {
    return ProgressError::InvalidTerminal;
  }
  return ProgressError::None;
}

ProgressError projectProgressForBook(const Progress& value, const std::uint32_t spineCount,
                                     const ResolvedChapterLayout& target, ProjectedProgress& out) noexcept {
  const auto error = validateProgressForBook(value, spineCount);
  if (error != ProgressError::None) return error;

  ProjectedProgress candidate;
  candidate.spineIndex = value.spineIndex;
  if (static_cast<std::uint32_t>(value.spineIndex) == spineCount) {
    candidate.terminal = true;
    candidate.approximate = false;
    out = candidate;
    return ProgressError::None;
  }
  if (!target.ready) return ProgressError::LayoutNotReady;
  candidate.chapterPageCount = target.pageCount;
  if (target.pageCount != 0U) {
    std::uint64_t page = value.chapterPage;
    if (value.chapterPageCount && *value.chapterPageCount != 0U) {
      page = (static_cast<std::uint64_t>(value.chapterPage) * target.pageCount) / *value.chapterPageCount;
    }
    const auto lastPage = static_cast<std::uint64_t>(target.pageCount) - 1U;
    if (page > lastPage) page = lastPage;
    candidate.chapterPage = static_cast<std::uint16_t>(page);
  }
  out = candidate;
  return ProgressError::None;
}

ProgressError encodeLegacyProgress(const Progress& value, std::uint8_t* bytes, const std::size_t capacity,
                                   std::size_t& written) noexcept {
  const auto error = validateProgressShape(value);
  if (error != ProgressError::None) return error;
  if ((!value.chapterPageCount && (value.finished || value.percent)) || (!value.finished && value.percent)) {
    return ProgressError::UnrepresentableLegacyState;
  }
  const std::size_t size = value.percent ? 8U : value.finished ? 7U : value.chapterPageCount ? 6U : 4U;
  if (bytes == nullptr) return ProgressError::NullBuffer;
  if (capacity < size) return ProgressError::BufferTooSmall;

  std::uint8_t candidate[8] = {};
  writeLe16(candidate, value.spineIndex);
  writeLe16(candidate + 2, value.chapterPage);
  if (value.chapterPageCount) writeLe16(candidate + 4, *value.chapterPageCount);
  if (value.finished) candidate[6] = *value.finished ? 1U : 0U;
  if (value.percent) candidate[7] = *value.percent;
  std::memcpy(bytes, candidate, size);
  written = size;
  return ProgressError::None;
}

}  // namespace sync
}  // namespace yomuka
