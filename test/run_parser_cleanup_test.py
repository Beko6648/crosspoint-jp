"""Exercise the real parser cleanup body with strict file/XML lifecycle doubles."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp').read_text(encoding='utf-8')
start = source.index('void ChapterHtmlSlimParser::closeIncrementalParser() {')
end = source.index('\nbool ChapterHtmlSlimParser::parseAndBuildPages()', start)
cleanup = source[start:end]
harness = r'''
#include <cassert>
int frees = 0;
void XML_ParserFree(void*) { ++frees; }
struct File {
  bool initialized = false, opened = false;
  int closes = 0;
  explicit operator bool() const { return initialized && opened; }
  void close() { assert(initialized); assert(opened); opened = false; ++closes; }
};
struct ChapterHtmlSlimParser {
  void* incrementalParser = nullptr;
  File incrementalFile;
  void closeIncrementalParser();
};
'''
cases = r'''
int main() {
  ChapterHtmlSlimParser p;
  // Cancel before first step, or failure before XML/file allocation.
  p.closeIncrementalParser(); p.closeIncrementalParser();
  assert(frees == 0 && p.incrementalFile.closes == 0);
  // XML exists, but opening the input file failed.
  p.incrementalParser = &p;
  p.incrementalFile.initialized = true;
  p.closeIncrementalParser(); p.closeIncrementalParser();
  assert(frees == 1 && p.incrementalParser == nullptr);
  assert(p.incrementalFile.closes == 0);
  // Completion or cancellation after opening, followed by destructor cleanup.
  p.incrementalParser = &p; p.incrementalFile.opened = true;
  p.closeIncrementalParser(); p.closeIncrementalParser();
  assert(frees == 2 && p.incrementalParser == nullptr);
  assert(p.incrementalFile.closes == 1);
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'cleanup.cpp'
    exe = Path(directory) / 'cleanup.exe'
    test.write_text(harness + cleanup + cases, encoding='utf-8')
    subprocess.run([args.compiler, 'c++', '-std=c++17', str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: unopened, failed-open, opened and repeated parser cleanup')
