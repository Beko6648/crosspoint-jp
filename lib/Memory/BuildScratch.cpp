#include "BuildScratch.h"

#include <Logging.h>

#include <atomic>

namespace buildscratch {
namespace {
uint8_t* block = nullptr;
size_t blockLength = 0;
std::atomic<bool> claimed{false};
}  // namespace

void lend(uint8_t* buffer, const size_t length) {
  if (block) {
    LOG_ERR("SCR", "Build scratch lent twice; ignoring second lend");
    return;
  }
  block = buffer;
  blockLength = length;
  claimed.store(false);
}

void reclaim() {
  if (claimed.load()) LOG_ERR("SCR", "Build scratch reclaimed while still claimed");
  block = nullptr;
  blockLength = 0;
  claimed.store(false);
}

uint8_t* claim(const size_t minimumLength, size_t* lengthOut) {
  if (!block || blockLength < minimumLength) return nullptr;
  bool expected = false;
  if (!claimed.compare_exchange_strong(expected, true)) return nullptr;
  if (lengthOut) *lengthOut = blockLength;
  return block;
}

void release(const uint8_t* buffer) {
  if (buffer && buffer == block) claimed.store(false);
}
}  // namespace buildscratch
