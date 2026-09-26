import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/chapter_input_lock';out.mkdir(parents=True,exist_ok=True)
s=(r/'src/activities/ActivityManager.cpp').read_text(encoding='utf-8')
start=s.index('RenderLock::RenderLock(TryLock)');end=s.index('void RenderLock::unlock()',start)
reader=(r/'src/activities/reader/EpubReaderActivity.cpp').read_text(encoding='utf-8')
rs=reader.index('void EpubReaderActivity::loop() {');re=reader.index('#if defined(IDLE_IMAGE_PREFETCH_TEST)',rs)
reader_body=reader[rs:re]+'}\n'
source=r'''#include <cassert>
#include <memory>
#define IDLE_CHAPTER_BUILD 1
#define LOG_INF(...) ((void)0)
#include "activities/RenderLock.h"
constexpr int pdTRUE=1;
struct {int renderingMutex=1;} activityManager;
bool held=false;int waits=0,gives=0;
int xSemaphoreTake(int,unsigned ticks){waits+=ticks;assert(ticks==0);if(held)return 0;held=true;return 1;}
void xSemaphoreGive(int){assert(held);held=false;++gives;}
'''+s[start:end]+r'''
class Activity {};
RenderLock::RenderLock(Activity&) { assert(false && "input must not take blocking lock"); }
unsigned millis(){return 42;}
struct Input {bool wasAnyPressed(){return true;}bool wasAnyReleased(){return false;}};
struct Build {~Build(){assert(held);}};
struct EpubReaderActivity:Activity {
 Input mappedInput;unsigned chapterLastInput=0;std::unique_ptr<Build> idleChapter;void loop();
};
'''+reader_body+r'''
int main(){
 held=true;
 {RenderLock lock(RenderLock::TryLock::Now);assert(!lock.ownsLock());}
 assert(held&&gives==0&&waits==0);
 held=false;
 {RenderLock lock(RenderLock::TryLock::Now);assert(lock.ownsLock());assert(held);}
 assert(!held&&gives==1&&waits==0);
 EpubReaderActivity reader;reader.idleChapter=std::make_unique<Build>();
 held=true;reader.loop();assert(reader.idleChapter&&reader.chapterLastInput==42&&held);
 held=false;reader.loop();assert(!reader.idleChapter&&!held&&waits==0);
}
'''
f=out/'input.cpp';f.write_text(source,encoding='utf-8');exe=out/'input.exe'
subprocess.run([a.compiler,'c++','-std=c++20','-I'+str(r/'src'),str(f),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS: production try-lock never waits or unlocks another owner; acquired lock released once')
