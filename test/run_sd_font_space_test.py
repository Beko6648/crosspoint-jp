#!/usr/bin/env python3
"""Exercise production SD-space fast paths and common ligature-cache release.

Compile extracted production methods with controlled cache/SD-metrics fixtures.
This reproduces full/partial tables and read failures without requiring an SD
card. Device font rendering and real SD timing remain separate acceptance checks.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--asan', action='store_true')
args = parser.parse_args()
gfx = (root / 'lib/GfxRenderer/GfxRenderer.cpp').read_text(encoding='utf-8')
font = (root / 'lib/EpdFont/SdCardFont.cpp').read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


width = gfx[gfx.index('int GfxRenderer::getSpaceWidth('):]
width = width[:width.index('  const int effectiveFontId')] + '  return -999;\n}\n'
advance = gfx[gfx.index('int GfxRenderer::getSpaceAdvance('):]
advance = advance[:advance.index('  const auto fontIt')] + '  return -999;\n}\n'
helper = function(gfx, 'uint16_t getSdCardSpaceAdvance(') if 'uint16_t getSdCardSpaceAdvance(' in gfx else ''
has_style = function(font, 'bool SdCardFont::hasAdvanceTable(uint8_t style)') if 'bool SdCardFont::hasAdvanceTable(uint8_t style)' in font else ''
source = r'''
#include <EpdFontData.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
struct EpdFontFamily { enum Style { REGULAR, BOLD, ITALIC, BOLD_ITALIC }; };
struct SdCardFont {
 static constexpr uint8_t MAX_STYLES=4;
 struct AdvanceEntry { uint32_t codepoint; uint16_t advanceX; };
 struct Header { uint16_t kernLeftEntryCount=1,kernRightEntryCount=1;
                 uint8_t kernLeftClassCount=1,kernRightClassCount=1,ligaturePairCount=1; };
 struct PerStyle {
  bool present=false;
  Header header;
  EpdFontData stubData{},miniData{};
  EpdKernClassEntry *kernLeftClasses=nullptr,*kernRightClasses=nullptr;
  int8_t *kernMatrix=nullptr,*kernRowCache=nullptr;
  EpdLigaturePair* ligaturePairs=nullptr;
  uint8_t kernCachedRows[4]{},kernRowCacheNext=0;
  bool kernRowCacheEnabled=false,kernLigLoaded=false;
 } styles_[4];
 std::vector<AdvanceEntry> tables[4];
 const AdvanceEntry* advanceTable_[4]{};
 uint32_t advanceTableSize_[4]{};
 uint16_t diskAdvance[4]{128,176,144,192};
 bool readFails=false;
 mutable int reads=0,lastStyle=-1;
 void table(int style, bool includesSpace, uint16_t value, int size=1) {
  auto& entries=tables[style]; entries.clear();
  if(includesSpace) entries.push_back({32,value});
  while(static_cast<int>(entries.size())<size) entries.push_back({uint32_t(100+entries.size()),value});
  advanceTable_[style]=entries.data();advanceTableSize_[style]=entries.size();
 }
 uint16_t readAdvanceOnly(uint32_t cp,uint8_t style) const {
  assert(cp==32);++reads;lastStyle=resolveStyle(style);
  return readFails ? 0 : diskAdvance[lastStyle];
 }
 uint8_t resolveStyle(uint8_t) const;
 uint16_t getAdvance(uint32_t,uint8_t) const;
 bool hasAdvanceTable() const;
 bool hasAdvanceTable(uint8_t) const;
 void freeStyleKernLigatureData(PerStyle&);
 void applyKernLigaturePointers(PerStyle&,EpdFontData&) const;
};
struct GfxRenderer {
 std::map<int,SdCardFont*> sdCardFonts_;
 uint16_t scale=256;
 uint16_t getSdCardFontScale(int) const {return scale;}
 int getSpaceWidth(int,EpdFontFamily::Style) const;
 int getSpaceAdvance(int,uint32_t,uint32_t,EpdFontFamily::Style) const;
};
''' + function(font, 'uint8_t SdCardFont::resolveStyle(') + function(font, 'uint16_t SdCardFont::getAdvance(') + function(font, 'bool SdCardFont::hasAdvanceTable()') + has_style + function(font, 'void SdCardFont::freeStyleKernLigatureData(') + function(font, 'void SdCardFont::applyKernLigaturePointers(') + helper + width + advance + r'''
int main() {
 unsigned checks=0, failures=0, widthFailures=0,styleFailures=0,releaseFailures=0;
 auto check=[&](bool ok){++checks;if(!ok)++failures;};
 // Full bounded table omits the space codepoint.
 {
  SdCardFont font; font.styles_[0].present=true;font.table(0,false,128,1024);
  GfxRenderer gfx;gfx.sdCardFonts_[1]=&font;
  bool ok=gfx.getSpaceWidth(1,EpdFontFamily::REGULAR)==8;
  check(ok);widthFailures+=!ok;
  check(gfx.getSpaceAdvance(1,'a','b',EpdFontFamily::REGULAR)==8);
 }
 // Cache styles independently: an existing Bold font must not use Regular metrics.
 {
  SdCardFont font;font.styles_[0].present=font.styles_[1].present=true;font.table(0,true,128);
  GfxRenderer gfx;gfx.sdCardFonts_[1]=&font;
  bool ok=gfx.getSpaceWidth(1,EpdFontFamily::BOLD)==11;
  check(ok);styleFailures+=!ok;
 }
 // Cache coverage, genuine zero advances, style fallback and read failures.
 for(unsigned present=1;present<16;++present) {
  for(int style=0;style<4;++style) {
   for(bool cached:{false,true}) {
    for(bool readFails:{false,true}) {
     SdCardFont font;
     for(int i=0;i<4;++i)font.styles_[i].present=(present&(1u<<i))!=0;
     auto resolved=font.resolveStyle(style);
     font.table(resolved,cached,font.diskAdvance[resolved]);font.readFails=readFails;
     GfxRenderer gfx;gfx.sdCardFonts_[1]=&font;
     int fp=(!cached&&readFails)?0:font.diskAdvance[resolved];
     check(gfx.getSpaceWidth(1,static_cast<EpdFontFamily::Style>(style))==fp4::toPixel(fp));
     for(uint16_t scale:{uint16_t(128),uint16_t(183),uint16_t(256),uint16_t(320)}) {
      gfx.scale=scale;
      int expected=fp4::toPixel(static_cast<int32_t>(int64_t(fp)*scale/256));
      check(gfx.getSpaceAdvance(1,'a','b',static_cast<EpdFontFamily::Style>(style))==expected);
     }
     if(cached)check(font.reads==0);
    }
   }
  }
 }
 // Preserve rounding for all normal cached metrics over scaled reader sizes.
 for(unsigned fp=1;fp<513;++fp) {
  SdCardFont font;font.styles_[0].present=true;font.table(0,true,fp);
  GfxRenderer gfx;gfx.sdCardFonts_[1]=&font;
  check(gfx.getSpaceWidth(1,EpdFontFamily::REGULAR)==fp4::toPixel(fp));
  for(uint16_t scale:{uint16_t(128),uint16_t(183),uint16_t(256),uint16_t(320)}) {
   gfx.scale=scale;
   check(gfx.getSpaceAdvance(1,'a','b',EpdFontFamily::REGULAR)==fp4::toPixel(int32_t(int64_t(fp)*scale/256)));
  }
  check(font.reads==0);
 }
 {
  SdCardFont font;font.styles_[0].present=true;font.diskAdvance[0]=0;font.table(0,true,0);
  GfxRenderer gfx;gfx.sdCardFonts_[1]=&font;
  check(gfx.getSpaceWidth(1,EpdFontFamily::REGULAR)==0);
  check(gfx.getSpaceAdvance(1,'a','b',EpdFontFamily::REGULAR)==0);
 }
 // Common release is also used by failures/reloads, not only the public cleanup.
 for(int cycle=0;cycle<100;++cycle) {
  SdCardFont font;auto& s=font.styles_[0];
  s.ligaturePairs=new EpdLigaturePair[1]{};
  s.kernLeftClasses=new EpdKernClassEntry[1]{};
  s.kernRightClasses=new EpdKernClassEntry[1]{};
  s.kernMatrix=new int8_t[1]{};s.kernRowCache=new int8_t[1]{};
  font.applyKernLigaturePointers(s,s.stubData);font.applyKernLigaturePointers(s,s.miniData);
  font.freeStyleKernLigatureData(s);
  bool ok=!s.stubData.ligaturePairs&&!s.miniData.ligaturePairs&&
          s.stubData.ligaturePairCount==0&&s.miniData.ligaturePairCount==0;
  check(ok);releaseFailures+=!ok;
  font.freeStyleKernLigatureData(s);
  check(!s.ligaturePairs&&!s.kernMatrix&&!s.kernRowCache);
  // Reload restores views from the unchanged on-disk header.
  s.ligaturePairs=new EpdLigaturePair[1]{};
  font.applyKernLigaturePointers(s,s.stubData);font.applyKernLigaturePointers(s,s.miniData);
  check(s.stubData.ligaturePairCount==1&&s.miniData.ligaturePairs==s.ligaturePairs);
  font.freeStyleKernLigatureData(s);
 }
 std::cout<<"checks="<<checks<<" failures="<<failures<<" full-cache="<<widthFailures
          <<" partial-style="<<styleFailures<<" release="<<releaseFailures<<"\n";
 if(EXPECT_BASELINE)assert(widthFailures>0&&styleFailures>0&&releaseFailures>0);
 else assert(failures==0);
}
'''
source = '#define EXPECT_BASELINE ' + str(int(args.baseline)) + '\n' + source
with tempfile.TemporaryDirectory(prefix='yomuka-sd-font-space-') as directory:
    path = Path(directory)
    cpp = path / 'test.cpp'
    cpp.write_text(source, encoding='utf-8')
    exe = path / ('test.exe' if os.name == 'nt' else 'test')
    command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-parameter', '-I'+str(root / 'lib/EpdFont')]
    if args.asan:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command + [str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
