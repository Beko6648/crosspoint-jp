#include <cassert>
#include <vector>

#include "BatchEpubPaths.h"

int main() {
  const std::vector<std::string> paths = {"/Book/羞贅卍＊餡.epub", "/nested/a b/😀記号.epub", "/Book/line\nbreak.epub"};
  {
    BatchEpubPaths list;
    assert(list.begin());
    for (const auto& path : paths) assert(list.append(path));
    assert(list.count() == 3);
    // Generation then summary/re-execution read the same complete snapshot.
    for (int pass = 0; pass < 3; ++pass) {
      assert(list.rewind());
      std::string path;
      for (const auto& expected : paths) {
        assert(list.next(path));
        assert(path == expected);
      }
      assert(!list.next(path));
    }
  }
  assert(!disk.exists);
  {
    BatchEpubPaths list;
    assert(list.begin());
    assert(list.rewind());
    assert(list.count() == 0);
    std::string path;
    assert(!list.next(path));
  }
  // Cancellation while scanning: the written prefix remains readable for summary.
  {
    BatchEpubPaths list;
    assert(list.begin());
    assert(list.append(paths[0]));
    assert(list.rewind());
    std::string path;
    assert(list.next(path) && path == paths[0]);
  }
  // Snapshot memory does not grow with the book count (storage double does).
  {
    BatchEpubPaths list;
    assert(list.begin());
    for (int i = 0; i < 500; ++i) assert(list.append(paths[i % paths.size()]));
    assert(list.count() == 500 && list.rewind());
    std::string path;
    for (int i = 0; i < 500; ++i) assert(list.next(path) && path == paths[i % paths.size()]);
  }
  for (int fault = 0; fault < 7; ++fault) {
    disk = {};
    BatchEpubPaths list;
    if (fault == 0) {
      disk.failOpen = true;
      assert(!list.begin());
      continue;
    }
    assert(list.begin());
    if (fault == 1) {
      disk.shortWrite = true;
      assert(!list.append(paths[0]));
      continue;
    }
    assert(list.append(paths[0]));
    if (fault == 2) {
      disk.failClose = true;
      assert(!list.rewind());
      continue;
    }
    if (fault == 3) {
      disk.failOpen = true;
      assert(!list.rewind());
      continue;
    }
    if (fault == 4) {
      disk.bytes.pop_back();
      assert(!list.rewind());
      continue;
    }
    if (fault == 5) {
      disk.failSeek = true;
      assert(!list.rewind());
      continue;
    }
    assert(list.rewind());
    disk.shortRead = true;
    std::string path;
    assert(!list.next(path));
  }
  disk = {};
  {
    BatchEpubPaths list;
    assert(list.begin());
    assert(list.append(paths[0]));
    assert(list.rewind());
    disk.bytes[0] = char(255);
    disk.bytes[1] = char(255);
    std::string path;
    assert(!list.next(path));
  }
  assert(!disk.exists);
}
