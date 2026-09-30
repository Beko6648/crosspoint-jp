"""Compile the actual specimen method against bounded renderer/font stubs."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/activities/settings/FontSelectionActivity.cpp').read_text(encoding='utf-8')
body = source[source.index('void FontSelectionActivity::drawSpecimen('):source.index('void FontSelectionActivity::render(')]
gfx = (root / 'lib/GfxRenderer/GfxRenderer.cpp').read_text(encoding='utf-8')
exact = gfx[gfx.index('void GfxRenderer::drawGlyphExact('):gfx.index('void GfxRenderer::renderChar(')]
cpp = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>
constexpr int UI_10_FONT_ID=100, NOTOSANS_14_FONT_ID=200;
enum class Language { JAPANESE, ENGLISH };
struct { Language getLanguage(){return Language::JAPANESE;} } I18N;
uint32_t utf8NextCodepoint(const uint8_t** s) {
 auto p=*s;uint32_t cp=*p++;if(cp>=0xc0){int n=cp<0xe0?1:2;cp&=n==1?31:15;while(n--)cp=(cp<<6)|(*p++&63);}*s=p;return cp;
}
namespace fp4 {int toPixel(int value){return value/16;}}
struct Glyph {int advanceX=224,left=0,width=14;} glyph;
struct EpdFontFamily {
 enum Style { REGULAR };
 bool latin=false;
 EpdFontFamily(bool value=false):latin(value){}
 EpdFontFamily(EpdFontFamily* f):latin(f->latin){}
 const Glyph* getGlyphExact(uint32_t cp)const{return cp<128||!latin?&glyph:nullptr;}
};
bool failLoad=false;
int resident=0;
struct SdCardFont {
 EpdFontFamily f{true};
 SdCardFont(){++resident;}~SdCardFont(){--resident;}
 bool load(const char*){return !failLoad;}
 EpdFontFamily* getEpdFont(){return &f;}
 bool hasCodepoint(uint32_t cp){return cp<128;}
};
struct SdCardFontFileInfo{std::string path;uint8_t pointSize;uint8_t style;};
struct Info {std::vector<SdCardFontFileInfo> files{{"test",18,0}};};
struct Registry {Info info;const Info* findFamily(const std::string&){return &info;}};
struct GfxRenderer {
 std::map<int,EpdFontFamily> fonts{{NOTOSANS_14_FONT_ID,EpdFontFamily{}}};
 const std::map<int,EpdFontFamily>& fontMap=fonts;
 std::map<int,SdCardFont*> sd;
 std::map<int,uint16_t> scales;
 mutable int sampleDraws=0,japaneseDraws=0;
 std::vector<std::string> labels;
 const auto& getFontMap()const{return fonts;}
 const auto& getSdCardFonts()const{return sd;}
 void insertFont(int id,EpdFontFamily f){fonts.emplace(id,f);}
 void removeFont(int id){fonts.erase(id);}
 void registerSdCardFont(int id,SdCardFont* f){sd[id]=f;}
 void unregisterSdCardFont(int id){sd.erase(id);}
 void registerSdCardFontScale(int id,uint16_t scale){scales[id]=scale;}
 void unregisterSdCardFontScale(int id){scales.erase(id);}
 int getSdCardFontScale(int id)const{auto it=scales.find(id);return it==scales.end()?256:it->second;}
 std::string truncatedText(int,const char* text,int){return text;}
 int getLineHeight(int id){return id==UI_10_FONT_ID?24:32;}
 int getTextWidth(int,const char* text){return uint8_t(*text)<128?14:28;}
 void getTextVisibleBoundsX(int id,const char* text,int* lo,int* hi){*lo=0;*hi=getTextWidth(id,text);}
 int getFontAscenderSize(int)const{return 24;}
 void drawGlyphExact(int,int,int,uint32_t)const;
 void renderChar(int,const EpdFontFamily&,uint32_t cp,int* x,const int* y,bool,EpdFontFamily::Style,bool fallback)const{
  assert(!fallback);assert(*x>=10&&*x+10<=410);assert(*y>=44&&*y-24+32<=340);
  ++sampleDraws;if(cp>=128)++japaneseDraws;
 }
 void drawText(int id,int x,int y,const char* text){
  if(id==UI_10_FONT_ID){labels.emplace_back(text);return;}
  assert(x>=10&&x+getTextWidth(id,text)<=410);assert(y>=20&&y+32<=340);
  ++sampleDraws;if(uint8_t(*text)>=128)++japaneseDraws;
 }
};
struct FontSelectionActivity {
 struct Entry{std::string name;bool isBuiltin;};
 GfxRenderer renderer;Registry registry;Registry* registry_=&registry;
 std::vector<Entry> fonts_{{"builtin",true},{"latin",false}};int selectedIndex_=0;
 void drawSpecimen(int,int,int,int);
};
''' + exact + body + r'''
int main(){
 FontSelectionActivity a;
 a.drawSpecimen(10,20,400,350);
 assert(a.renderer.sampleDraws>0&&a.renderer.japaneseDraws>0);
 assert(a.renderer.fonts.size()==1&&a.renderer.sd.empty()&&a.renderer.scales.empty()&&resident==0);
 a.selectedIndex_=1;a.renderer.sampleDraws=a.renderer.japaneseDraws=0;
 for(int i=0;i<20;++i)a.drawSpecimen(10,20,400,350);
 assert(a.renderer.sampleDraws>0&&a.renderer.japaneseDraws==0);
 assert(a.renderer.fonts.size()==1&&a.renderer.sd.empty()&&a.renderer.scales.empty()&&resident==0);
 failLoad=true;a.renderer.sampleDraws=0;a.drawSpecimen(10,20,400,350);
 assert(a.renderer.sampleDraws==0&&resident==0&&a.renderer.fonts.size()==1);
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(cpp,encoding='utf-8')
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: built-in specimen, missing Japanese blank, repeated SD preview cleanup, load failure')
