#pragma once

#include <cstddef>
#include <cstdint>

namespace buildscratch {
void lend(uint8_t* buffer, size_t length);
void reclaim();
uint8_t* claim(size_t minimumLength, size_t* lengthOut = nullptr);
void release(const uint8_t* buffer);
}  // namespace buildscratch
