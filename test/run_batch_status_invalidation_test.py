import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/batch_status_invalidation';out.mkdir(parents=True,exist_ok=True)
s=(r/'src/ReadingStatusHelper.cpp').read_text();a0=s.index('bool invalidateBookListStatusIndex(');b=s.index('void invalidateBookListStatusIndexEntry(',a0)
body=s[a0:b]
g=(r/'src/activities/settings/GenerateAllCacheActivity.cpp').read_text();g=g[g.index('void GenerateAllCacheActivity::generateAllCaches()'):]
assert g.index('invalidateBookListStatusIndex(')<g.index('findEpubFiles(')<g.index('clearFullCacheGeneratedMarker(')
source=r"""
#include <cassert>
#include <string>
#include <set>
constexpr char BOOK_LIST_STATUS_INDEX_FILE[]="/book-list-status.bin";
struct Store {std::set<std::string> files; bool fail=false; int removes=0;
bool exists(const char* p){return files.count(p);}
bool remove(const char* p){++removes;if(fail)return false;return files.erase(p);}} Storage;
"""+body+r"""
int main(){
 Storage.files={"/.crosspoint/book-list-status.bin","/.crosspoint/epub_1/.full_cache_complete","/.crosspoint/epub_1/sections/0.bin","/.crosspoint/epub_1/progress.bin"};
 assert(invalidateBookListStatusIndex("/.crosspoint"));assert(Storage.files.size()==3);assert(Storage.removes==1);
 assert(invalidateBookListStatusIndex("/.crosspoint"));assert(Storage.removes==1);
 Storage.files.insert("/.crosspoint/book-list-status.bin");Storage.fail=true;
 assert(!invalidateBookListStatusIndex("/.crosspoint"));assert(Storage.files.size()==4);
}
"""
f=out/'test.cpp';f.write_text(source);exe=out/'test.exe'
subprocess.run([a.compiler,'c++','-std=c++20',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: only derived summary invalidated before scanning/mutations; book caches/progress preserved; absent index and storage failure handled')
