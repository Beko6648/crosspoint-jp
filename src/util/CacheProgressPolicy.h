#pragma once
#include <cstdint>

// Check at book/section boundaries: keep e-paper updates bounded, but never
// wait for 25% of the entire library before showing a new chapter.
class CacheProgressPolicy {
 public:
  explicit CacheProgressPolicy(uint32_t now) : lastUpdate(now) {}
  bool shouldUpdate(int book, int percent, uint32_t now) const {
    return book != lastBook || percent >= lastPercent + 25 || uint32_t(now - lastUpdate) >= 5000;
  }
  void displayed(int book, int percent, uint32_t now) {
    lastBook = book;
    lastPercent = percent;
    lastUpdate = now;
  }

 private:
  int lastBook = -1;
  int lastPercent = 0;
  uint32_t lastUpdate;
};
