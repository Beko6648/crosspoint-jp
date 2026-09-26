import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/image_cancel';out.mkdir(parents=True,exist_ok=True)
(out/'Print.h').write_text('#pragma once\n#include <cstdint>\n#include <cstddef>\nstruct Print { virtual size_t write(uint8_t)=0; virtual size_t write(const uint8_t*,size_t)=0; virtual ~Print()=default; };\n')
s=(r/'lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp').read_text()
start=s.index('            if (imageOutput.isCancelled())');end=s.index('            if (extractSuccess)',start)
cleanup=s[start:end]
source=r"""
#include <cassert>
#include <string>
#include "Epub/CancellableImageOutput.h"
#define LOG_INF(...) ((void)0)
struct Sink:Print { int bytes=0;bool shortWrite=false;size_t write(uint8_t c)override{return write(&c,1);}size_t write(const uint8_t*,size_t n)override{if(shortWrite)return 0;bytes+=n;return n;} };
struct Store {bool removed=false;bool remove(const char*){removed=true;return true;}} Storage;
struct Parser {bool lowMemoryAbortRequested=false;};
void callback(CancellableImageOutput& imageOutput,Parser* self,bool& placeholder) {
 std::string cachedImagePath="partial.jpg";
"""+cleanup+r"""
 placeholder=true;
}
int main(){
 uint8_t data[8]={}; Sink sink;bool cancel=false;int polls=0;
 std::function<bool()> fn=[&]{++polls;return cancel;};
 CancellableImageOutput output(sink,fn);
 assert(output.write(data,8)==8&&sink.bytes==8);
 cancel=true;assert(output.write(data,8)==0&&sink.bytes==8);
 cancel=false;assert(output.isCancelled());int before=polls;assert(output.write(data,8)==0&&polls==before);
 Parser parser;bool placeholder=false;callback(output,&parser,placeholder);
 assert(Storage.removed&&parser.lowMemoryAbortRequested&&!placeholder);
 Sink plain;std::function<bool()> none;CancellableImageOutput normal(plain,none);
 assert(normal.write(data,8)==8&&!normal.isCancelled());
 plain.shortWrite=true;assert(normal.write(data,8)==0&&!normal.isCancelled());
 // Last output succeeded, then input arrives before decoder/placeholder processing.
 Sink last;cancel=false;CancellableImageOutput tail(last,fn);assert(tail.write(data,8)==8);
 cancel=true;Storage.removed=false;parser.lowMemoryAbortRequested=false;placeholder=false;
 callback(tail,&parser,placeholder);assert(Storage.removed&&parser.lowMemoryAbortRequested&&!placeholder);
}
"""
f=out/'test.cpp';f.write_text(source);exe=out/'test.exe'
subprocess.run([a.compiler,'c++','-std=c++20','-I'+str(out),'-I'+str(r/'lib/Epub'),str(f),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS: chunk cancellation latched, no further writes, short-write distinguished, production callback removes partial image and aborts before placeholder, last-chunk cancellation')
