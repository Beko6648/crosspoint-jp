#pragma once

#include <BuildScratch.h>
#include <GfxRenderer.h>

#include <memory>
#include <new>
#include <optional>

// Owns either a heap object or a placement-new object in an exclusive scratch
// claim. A locally acquired framebuffer loan outlives the object and its claim.
template <typename Decoder>
class LoanedDecoder {
 public:
  LoanedDecoder() = default;
  LoanedDecoder(const LoanedDecoder&) = delete;
  LoanedDecoder& operator=(const LoanedDecoder&) = delete;
  ~LoanedDecoder() {
    if (scratch_) {
      decoder_->~Decoder();
      buildscratch::release(scratch_);
    } else {
      delete decoder_;
    }
    // loan_ is destroyed only after the decoder and claim are gone.
  }

  bool tryHeap() {
    if (decoder_) return true;
    decoder_ = new (std::nothrow) Decoder();
    return decoder_ != nullptr;
  }

  bool tryLoan(GfxRenderer& renderer, bool& framebufferInvalidated) {
    if (decoder_) return true;
    if (!buildscratch::isLent()) {
      if (!renderer.hasFrameBuffer()) return false;
      loan_.emplace(renderer);
      // Returning even an unused loan clears RAM framebuffer contents.
      framebufferInvalidated = true;
    }
    size_t capacity = 0;
    uint8_t* claim = buildscratch::claim(sizeof(Decoder), &capacity);
    if (!claim) return false;  // never reclaim another owner's loan/claim
    void* aligned = claim;
    size_t usable = capacity;
    if (!std::align(alignof(Decoder), sizeof(Decoder), aligned, usable)) {
      buildscratch::release(claim);
      return false;
    }
    scratch_ = claim;
    capacity_ = usable;
    decoder_ = new (aligned) Decoder();
    return true;
  }

  Decoder* operator->() const { return decoder_; }
  Decoder& operator*() const { return *decoder_; }
  explicit operator bool() const { return decoder_ != nullptr; }
  bool usesScratch() const { return scratch_ != nullptr; }
  size_t capacity() const { return capacity_; }

 private:
  std::optional<GfxRenderer::FrameBufferLoan> loan_;
  uint8_t* scratch_ = nullptr;
  Decoder* decoder_ = nullptr;
  size_t capacity_ = 0;
};
