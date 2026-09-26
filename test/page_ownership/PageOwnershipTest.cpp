#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <Serialization.h>

#include <cassert>
#include <iostream>
#include <new>

// Stub payload codecs so Page's actual rejection and ownership paths can be tested.
static bool missingPayload = false;
static size_t failSize = 0;
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return size == failSize ? nullptr : ::operator new(size);
}
ImageBlock::ImageBlock(const std::string& path, int16_t w, int16_t h) : imagePath(path), width(w), height(h) {}
void ImageBlock::render(GfxRenderer&, int, int) {}
bool ImageBlock::serialize(FsFile&) { return true; }
std::unique_ptr<ImageBlock> ImageBlock::deserialize(FsFile&) {
  if (missingPayload) return {};
  return std::unique_ptr<ImageBlock>(new ImageBlock("test", 20, 30));
}
void TextBlock::render(GfxRenderer&, int, int, int, int, int, int, int, int, int) const {}
void TextBlock::collectCodepoints(std::vector<uint32_t>&, size_t) const {}
void TextBlock::appendRubyText(std::string&) const {}
bool TextBlock::serialize(FsFile&) const { return true; }
std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile&) {
  if (missingPayload) return {};
  return std::unique_ptr<TextBlock>(new TextBlock({}, {}, {}, {}));
}

FsFile fixture(uint8_t tag) {
  FsFile f;
  f.bytes = std::make_shared<std::vector<uint8_t>>();
  serialization::writePod(f, uint16_t{1});
  serialization::writePod(f, tag);
  serialization::writePod(f, int16_t{7});
  serialization::writePod(f, int16_t{9});
  serialization::writePod(f, uint16_t{0});
  return f;
}
struct CountingElement final : PageElement {
  int& deaths;
  explicit CountingElement(int& n) : PageElement(0, 0), deaths(n) {}
  ~CountingElement() override { ++deaths; }
  void render(GfxRenderer&, int, int, int, int, int, int, int) override {}
  bool serialize(FsFile&) override { return true; }
  PageElementTag getTag() const override { return TAG_PageLine; }
};
int main() {
  for (uint8_t tag : {uint8_t(TAG_PageLine), uint8_t(TAG_PageImage)}) {
    auto f = fixture(tag);
    auto page = Page::deserialize(f);
    assert(page && page->elements.size() == 1);
    assert(page->elements[0]->xPos == 7 && page->elements[0]->yPos == 9);
    FsFile roundtrip;
    roundtrip.bytes = std::make_shared<std::vector<uint8_t>>();
    assert(page->serialize(roundtrip));
    assert(*roundtrip.bytes == *f.bytes);
    missingPayload = true;
    f = fixture(tag);
    assert(!Page::deserialize(f));
    missingPayload = false;
    failSize = tag == TAG_PageLine ? sizeof(PageLine) : sizeof(PageImage);
    f = fixture(tag);
    assert(!Page::deserialize(f));
    failSize = 0;
    for (size_t size = 0; size < 9; ++size) {
      f = fixture(tag);
      f.bytes->resize(size);
      assert(!Page::deserialize(f));
    }
  }
  auto f = fixture(TAG_PageTableRow);
  (*f.bytes)[7] = 201;  // Invalid cell count rejected by production TableRowBlock codec.
  assert(!Page::deserialize(f));
  f = fixture(255);
  assert(!Page::deserialize(f));
  failSize = sizeof(Page);
  f = fixture(TAG_PageImage);
  assert(!Page::deserialize(f));
  failSize = 0;
  // Destroy an earlier page while a later row still needs the shared column layout.
  GfxRenderer renderer;
  auto layout = std::make_shared<TableColumnLayout>();
  layout->colWidths = {80};
  layout->fontId = 1;
  layout->lineHeight = 20;
  std::weak_ptr<TableColumnLayout> weakLayout = layout;
  auto first = std::make_unique<Page>();
  auto second = std::make_unique<Page>();
  for (auto* page : {first.get(), second.get()}) {
    auto row = std::make_unique<TableRowBlock>(std::vector<std::vector<std::string>>{{"row"}}, std::vector<bool>{false},
                                               layout, 25, true, true);
    page->elements.push_back(std::make_unique<PageTableRow>(std::move(row), 0, 0));
  }
  layout.reset();
  first.reset();
  assert(!weakLayout.expired());
  second->render(renderer, 1, 0, 0, 100);
  assert(!renderer.draws.empty() && renderer.draws.back().text == "row");
  FsFile table;
  table.bytes = std::make_shared<std::vector<uint8_t>>();
  assert(second->serialize(table));
  auto restored = Page::deserialize(table);
  assert(restored && restored->elements.size() == 1);
  restored->render(renderer, 1, 0, 0, 100);
  second.reset();
  assert(weakLayout.expired());
  int deaths = 0;
  {
    Page page;
    auto element = std::make_unique<CountingElement>(deaths);
    page.elements.push_back(std::move(element));
    assert(!element && deaths == 0);
  }
  assert(deaths == 1);
  std::cout << "Page ownership, null payload/allocation rejection, truncated headers and format roundtrip: PASS\n";
}
