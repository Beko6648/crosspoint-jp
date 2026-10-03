import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/chapter_exit';out.mkdir(parents=True,exist_ok=True)
s=(r/'src/activities/reader/EpubReaderActivity.cpp').read_text(encoding='utf-8')
start=s.index('void EpubReaderActivity::onExit() {');end=s.index('  READING_HISTORY.endSession();',start)
body=s[start:end]+'}\n'
source=r'''#include <cassert>
#include <atomic>
#include <memory>
#define IDLE_CHAPTER_BUILD 1
bool held=false; int releases=0;
struct RenderLock {
 RenderLock(){assert(!held);held=true;}
 template<class T> explicit RenderLock(T&):RenderLock(){}
 ~RenderLock(){held=false;}
};
struct Build {~Build(){assert(held);++releases;}};
struct Activity {void onExit(){assert(held);}};
struct EpubReaderActivity:Activity {
 std::unique_ptr<Build> idleChapter;
 std::atomic<unsigned> chapterRenderReady{123};
 unsigned rememberCalls=0;void rememberBookOnceRendered(){++rememberCalls;}
 void onExit();
};
'''+body+r'''
int main(){
 for(bool active:{false,true}){
  EpubReaderActivity reader;
  if(active) reader.idleChapter=std::make_unique<Build>();
  {RenderLock managerLock;reader.onExit();assert(held);assert(!reader.idleChapter);assert(reader.chapterRenderReady.load()==0);}
  assert(!held&&reader.rememberCalls==1);
 }
 assert(releases==1);
}
'''
f=out/'exit.cpp';f.write_text(source,encoding='utf-8');exe=out/'exit.exe'
subprocess.run([a.compiler,'c++','-std=c++20',str(f),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS: production onExit cleanup under manager-held nonrecursive lock, active and inactive build')
