"""Run actual EPUB boundary methods with a strict spine lookup double."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'lib/Epub/Epub.cpp').read_text(encoding='utf-8')
def method(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]
methods = method('int Epub::getTocIndexForSpineIndex(') + '\n' + method('float Epub::calculateProgress(')
harness = r'''
#include <cassert>
#include <cstddef>
#include <vector>
struct Epub {
  struct Entry { int tocIndex; size_t cumulativeSize; };
  std::vector<Entry> entries;
  mutable int lookups = 0;
  int getSpineItemsCount() const { return entries.size(); }
  Entry getSpineItem(int i) const {
    assert(i >= 0 && i < getSpineItemsCount()); ++lookups; return entries[i];
  }
  size_t getBookSize() const { return entries.empty() ? 0 : entries.back().cumulativeSize; }
  size_t getCumulativeSpineItemSize(int i) const { return getSpineItem(i).cumulativeSize; }
  int getTocIndexForSpineIndex(int) const;
  float calculateProgress(int, float) const;
};
'''
cases = r'''
int main() {
  Epub book{{{0, 100}, {2, 200}, {-1, 300}, {5, 400}}};
  assert(book.calculateProgress(0, 0) == 0);
  assert(book.calculateProgress(1, 0.5f) == 0.375f);
  assert(book.calculateProgress(3, 1) == 1);
  assert(book.getTocIndexForSpineIndex(1) == 2);
  assert(book.getTocIndexForSpineIndex(2) == -1);
  book.lookups = 0;
  for (int i : {4, 5, 100}) {
    assert(book.calculateProgress(i, 0) == 1);
    assert(book.getTocIndexForSpineIndex(i) == -1);
  }
  assert(book.calculateProgress(-1, 0) == 0);
  assert(book.getTocIndexForSpineIndex(-1) == -1);
  assert(book.lookups == 0);
  Epub empty;
  assert(empty.calculateProgress(0, 0) == 0);
  assert(empty.getTocIndexForSpineIndex(0) == -1);
  Epub one{{{0, 100}}};
  assert(one.calculateProgress(0, 0.5f) == 0.5f);
  assert(one.calculateProgress(1, 0) == 1);
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'boundary.cpp'
    exe = Path(directory) / 'boundary.exe'
    test.write_text(harness + methods + cases, encoding='utf-8')
    subprocess.run([args.compiler, 'c++', '-std=c++17', str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: normal chapters, end-of-book, invalid positions, empty and single-chapter books')
