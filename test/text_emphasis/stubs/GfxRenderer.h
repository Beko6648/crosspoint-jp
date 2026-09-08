#pragma once
#include "EpdFontFamily.h"
#include "FontCacheManager.h"
#include <Utf8.h>
#include <vector>
class GfxRenderer {
 public:
  std::map<int,EpdFontFamily> fonts{{1,{}}};
  FontCacheManager cache;
  static constexpr int W=900,H=160;
  std::vector<uint8_t> pixels=std::vector<uint8_t>(W*H,255);
  // Diagonal stroke: distinguishes the selected font glyph from a geometric circle.
  uint8_t bitmap[4]={0x82,0x08,0x20,0x80};
  const auto& getFontMap() const { return fonts; }
  const uint8_t* getGlyphBitmap(const EpdFontData*, const EpdGlyph*) const { return bitmap; }
  FontCacheManager* getFontCacheManager() { return &cache; }
  int getLineHeight(int) const { return 25; }
  int getTextAdvanceX(int,const char* text,EpdFontFamily::Style) const {
    auto* p=reinterpret_cast<const unsigned char*>(text); int width=0;
    while(uint32_t cp=utf8NextCodepoint(&p)) width += cp<128?10:25;
    return width;
  }
  void drawPixel(int x,int y,bool) { if(x>=0&&x<W&&y>=0&&y<H) pixels[y*W+x]=0; }
};
