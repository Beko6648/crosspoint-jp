#pragma once
#include <algorithm>
namespace yomuka::sync {
enum class ReviewButton { Previous, Next, Confirm };
struct ReviewResult {
  int page;
  bool execute;
};
inline int finalReviewPage(int lines, int rows) { return (lines + std::max(1, rows) - 1) / std::max(1, rows); }
inline ReviewResult reviewInput(int page, int finalPage, ReviewButton button) {
  page = std::clamp(page, 0, finalPage);
  if (button == ReviewButton::Previous) return {std::max(0, page - 1), false};
  if (button == ReviewButton::Next) return {std::min(finalPage, page + 1), false};
  return {page, page == finalPage};
}
}  // namespace yomuka::sync
