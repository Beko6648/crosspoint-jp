#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// Pure conversion helpers for the frozen snapshot-v1 progress contract.
// No filesystem, JSON parsing, renderer, global settings or heap allocation.
namespace yomuka {
namespace sync {

struct Progress {
  std::uint16_t spineIndex = 0;
  std::uint16_t chapterPage = 0;
  std::optional<std::uint16_t> chapterPageCount;
  std::optional<bool> finished;
  std::optional<std::uint8_t> percent;
};

enum class ProgressError : std::uint8_t {
  None,
  UnsupportedLength,
  NullBuffer,
  InvalidFinishedFlag,
  InvalidPercent,
  InvalidPageRange,
  InvalidSpineCount,
  SpineOutOfRange,
  InvalidTerminal,
  LayoutNotReady,
  UnrepresentableLegacyState,
  BufferTooSmall,
};

const char* progressErrorName(ProgressError error) noexcept;

// The future JSON reader MUST validate integer types/ranges BEFORE narrowing
// to this structure. This function cannot detect an already-truncated integer.
ProgressError validateProgressShape(const Progress& value) noexcept;

// bytes must point to at least size readable bytes. The caller must obtain the
// ACTUAL file length and complete read result; an 8-byte prefix of a longer
// file is not a valid record. Accepts ONLY 4/6/7/8 bytes; 255 means unknown
// percent. Empty/missing files are NOT synthesized as progress. out is unchanged
// on every failure. Do not replace the reader's legacy-compatible loader.
ProgressError decodeLegacyProgress(const std::uint8_t* bytes, std::size_t size, Progress& out) noexcept;

// Checks the snapshot's source position against the resolved, same-BookId EPUB.
// finished=true alone is never interpreted as the end of the book. At spine N,
// only the exact v1 terminal tuple is accepted. No storage writes occur.
ProgressError validateProgressForBook(const Progress& value, std::uint32_t spineCount) noexcept;

struct ResolvedChapterLayout {
  // Set only after successful reflow using the receiver's final settings.
  // A failed/unfinished parser is not an empty (zero-page) chapter.
  bool ready = false;
  std::uint16_t pageCount = 0;
};

struct ProjectedProgress {
  std::uint16_t spineIndex = 0;
  std::uint16_t chapterPage = 0;
  std::uint16_t chapterPageCount = 0;
  bool terminal = false;
  // Equal page counts still do not prove equal text positions/layout settings.
  bool approximate = true;
};

// Ratio/clamp projection, not a text anchor or CFI. Unknown finished/percent
// are deliberately NOT invented here. Reflow, receiver-side recomputation and
// transactional storage remain caller responsibilities. out is unchanged on
// failure. An exact terminal tuple needs no chapter layout.
ProgressError projectProgressForBook(const Progress& value, std::uint32_t spineCount,
                                     const ResolvedChapterLayout& target, ProjectedProgress& out) noexcept;

// Canonical minimal 4/6/7/8-byte encoding. Some legal wire-level null
// combinations cannot be represented by the old on-disk format; reject them
// instead of turning unknown into zero/false. An 8-byte percent=255 input
// canonicalizes to a 7-byte output after decode. This does NOT write a file.
// bytes and written are unchanged on failure. On success only written bytes
// are touched. Source value, output buffer and written must not alias.
ProgressError encodeLegacyProgress(const Progress& value, std::uint8_t* bytes, std::size_t capacity,
                                   std::size_t& written) noexcept;

}  // namespace sync
}  // namespace yomuka
