#include <Serialization.h>

#include <cassert>
#include <vector>

enum Style : uint8_t { Regular = 0, Bold = 1, Italic = 2, BoldItalic = 3 };

template <typename T>
void compare(const std::vector<T>& values) {
  FsFile oldFile, newFile;
  for (auto value : values) serialization::writePod(oldFile, value);
  assert(serialization::writePodArray(newFile, values.data(), values.size()));
  assert(oldFile.bytes == newFile.bytes);
  assert(newFile.calls == (values.empty() ? 0u : 1u));
}

int main() {
  compare<int16_t>({});
  compare<int16_t>({-32768, -1, 0, 1, 32767});
  compare<Style>({Regular, Bold, Italic, BoldItalic});
  std::vector<int16_t> positions(2000);
  for (size_t i = 0; i < positions.size(); ++i) positions[i] = static_cast<int16_t>(i * 13);
  compare(positions);
  FsFile failed;
  failed.shortWrite = true;
  assert(!serialization::writePodArray(failed, positions.data(), positions.size()));
  FsFile overflow;
  assert(!serialization::writePodArray(overflow, positions.data(), std::numeric_limits<size_t>::max()));
  assert(overflow.calls == 0);
}
