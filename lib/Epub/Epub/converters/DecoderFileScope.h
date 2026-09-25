#pragma once

// JPEGDEC and PNG do not close callback-owned files in their destructors.
// Construct immediately after open(), including an unsuccessful header parse.
// Ownership of the decoder itself stays separate for future placement-new loans.
template <typename Decoder>
class DecoderFileScope {
 public:
  explicit DecoderFileScope(Decoder& decoder) : decoder_(decoder) {}
  ~DecoderFileScope() { decoder_.close(); }
  DecoderFileScope(const DecoderFileScope&) = delete;
  DecoderFileScope& operator=(const DecoderFileScope&) = delete;

 private:
  Decoder& decoder_;
};
