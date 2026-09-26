#include <expat.h>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cstdio>
#define LOG_ERR(...) ((void)0)
constexpr size_t PARSE_BUFFER_SIZE=1024;
constexpr size_t MIN_SIZE_FOR_POPUP=100;
constexpr size_t MIN_FREE_HEAP_FOR_PARSING=16000;
struct { unsigned free=100000; unsigned getFreeHeap() const {return free;} } ESP;
struct FsFile {
  std::string data; size_t pos=0; bool opened=false, readError=false;
  size_t size() const {return data.size();}
  size_t available() const {return opened?data.size()-pos:0;}
  size_t read(void* p,size_t n) {if(readError)return 0;n=std::min(n,available());memcpy(p,data.data()+pos,n);pos+=n;return n;}
  void close(){opened=false;}
};
struct {std::string data;bool fail=false;bool openFileForRead(const char*,const std::string&,FsFile& f){if(fail)return false; f.data=data;f.pos=0;f.opened=true;return true;}} Storage;
enum class CssTextAlign {None,Justify};
struct BlockStyle {bool textAlignDefined;CssTextAlign alignment;};
struct Text {};
struct Page {std::vector<int> elements{1};};
class ChapterHtmlSlimParser {
 public:
  XML_Parser incrementalParser=nullptr; FsFile incrementalFile;bool incrementalFinished=false;
  std::string filepath="book", text; std::vector<std::string> events;
  bool htmlEnded=false,lowMemoryAbortRequested=false;
  uint8_t paragraphAlignment=0;std::function<bool()> cancelFn;std::function<void()> popupFn;
  std::unique_ptr<Text> currentTextBlock;std::unique_ptr<Page> currentPage;
  std::string pendingAnchorId;std::vector<std::pair<std::string,uint16_t>> anchorData;
  int completedPageCount=0,flushes=0;
  enum class StepResult {Pending,Complete,Failed};
  ~ChapterHtmlSlimParser();void closeIncrementalParser();bool parseAndBuildPages();StepResult stepParseAndBuildPages();
  void startNewTextBlock(const BlockStyle&){currentTextBlock=std::make_unique<Text>();currentPage=std::make_unique<Page>();}
  void makePages(){++flushes;}
  void completeCurrentPage(){++completedPageCount;}
  static void XMLCALL defaultHandlerExpand(void*,const char*,int){}
  static void XMLCALL startElement(void* c,const char* n,const char**){static_cast<ChapterHtmlSlimParser*>(c)->events.emplace_back(n);}
  static void XMLCALL endElement(void* c,const char* n){auto* p=static_cast<ChapterHtmlSlimParser*>(c);p->events.push_back(std::string("/")+n);if(std::string(n)=="html")p->htmlEnded=true;}
  static void XMLCALL characterData(void* c,const char* s,int n){static_cast<ChapterHtmlSlimParser*>(c)->text.append(s,n);}
};
#include "production_driver.inc"
int main(){
 Storage.data="<html><body>";
 for(int i=0;i<900;++i)Storage.data+="<p id='x'>日本語<ruby>漢字<rt>かんじ</rt></ruby>&amp;<b>太字</b></p>";
 Storage.data+="</body></html>";
 ChapterHtmlSlimParser sync;assert(sync.parseAndBuildPages());
 ChapterHtmlSlimParser sliced;int steps=0;
 while(true){auto result=sliced.stepParseAndBuildPages();++steps;if(result==ChapterHtmlSlimParser::StepResult::Complete)break;assert(result==ChapterHtmlSlimParser::StepResult::Pending);assert(sliced.incrementalFile.opened);}
 assert(steps>10 && sliced.text==sync.text && sliced.events==sync.events && sliced.flushes==1);
 assert(!sliced.incrementalFile.opened && !sliced.incrementalParser);
 assert(sliced.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Failed);
 {ChapterHtmlSlimParser p;bool cancel=false;p.cancelFn=[&]{return cancel;};assert(p.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Pending);cancel=true;assert(p.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Failed);assert(!p.incrementalParser&&!p.incrementalFile.opened&&p.flushes==0);}
 {ChapterHtmlSlimParser p;ESP.free=1;assert(p.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Failed);assert(!p.incrementalParser);ESP.free=100000;}
 {ChapterHtmlSlimParser p;Storage.fail=true;assert(!p.parseAndBuildPages());assert(!p.incrementalParser);Storage.fail=false;}
 {ChapterHtmlSlimParser p;assert(p.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Pending);p.incrementalFile.readError=true;assert(p.stepParseAndBuildPages()==ChapterHtmlSlimParser::StepResult::Failed);}
 Storage.data="<html><body><p>broken</body></html>";{ChapterHtmlSlimParser p;assert(!p.parseAndBuildPages());assert(!p.incrementalParser);}
 Storage.data="<html><body>ok</body></html>tail";{ChapterHtmlSlimParser p;assert(p.parseAndBuildPages());assert(p.text=="ok");}
 Storage.data="";{ChapterHtmlSlimParser p;assert(!p.parseAndBuildPages());}
 puts("PASS: production incremental driver, real Expat, UTF-8/ruby events, sync equivalence, cancellation, low heap, IO, malformed/trailing XML");
}
