#!/usr/bin/env python3
"""Compile the production resume gate and all three persistence methods.

Controlled render completion and storage spies cover early exit, failed first
display, repeated display, and BookID preservation. Real rendering/SD failures
and sleep remain device checks.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
reader = root / 'src/activities/reader'


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


methods = ''
for kind, member in [('Epub', 'epub'), ('Txt', 'txt'), ('Xtc', 'xtc')]:
    text = (reader / (kind + 'ReaderActivity.cpp')).read_text(encoding='utf-8')
    cls = kind + 'ReaderActivity'
    enter = function(text, 'void ' + cls + '::onEnter()')
    assert 'APP_STATE.openEpubPath =' not in enter
    assert 'RECENT_BOOKS.addBook' not in enter
    assert 'clearRememberedBook()' in enter
    assert 'rememberBookOnceRendered()' in function(text, 'void ' + cls + '::loop()')
    exit_body = function(text, 'void ' + cls + '::onExit()')
    assert exit_body.index('rememberBookOnceRendered()') < exit_body.index(member + '.reset()')
    methods += function(text, 'void ' + cls + '::rememberBookOnceRendered()') + '\n'

source = r'''
#include "ReaderResumeState.h"
#include "CrossPointState.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#define LOG_ERR(...) ((void)0)
constexpr int UI_12_FONT_ID = 1, STR_EMPTY_FILE = 1, STR_PAGE_LOAD_ERROR = 2, STR_BACK = 3;
const char* tr(int) { return "message"; }
struct EpdFontFamily { enum { BOLD }; };
struct RenderLock {};
struct Renderer {
 void clearScreen() {}
 void drawCenteredText(int, int, const char*, bool, int) {}
 void displayBuffer() {}
};
struct MappedInput {
 struct Labels { const char *btn1, *btn2, *btn3, *btn4; };
 Labels mapLabels(const char* a, const char* b, const char* c, const char* d) { return {a,b,c,d}; }
};
struct Gui { void drawButtonHints(Renderer&, const char*, const char*, const char*, const char*) {} } GUI;
struct RecentBooks {
 int writes = 0; std::string path; uint64_t bookId = 0;
 void addBook(const std::string& p, const std::string&, const std::string&,
              const std::string&, uint64_t id = 0) { ++writes; path = p; bookId = id; }
} RECENT_BOOKS;
struct Book {
 std::string path = "/fixture.epub";
 size_t fileSize = 100, contentOffset = 0;
 size_t getFileSize() const { return fileSize; }
 size_t getContentOffset() const { return contentOffset; }
 const std::string& getPath() const { return path; }
 std::string getTitle() const { return "Fixture"; }
 std::string getAuthor() const { return "Author"; }
 std::string getThumbBmpPath() const { return "thumb.bmp"; }
 bool getSourceFingerprint(uint64_t* id) const { *id = 0xFEDCBA9876543210ULL; return true; }
};
struct EpubReaderActivity { ReaderResumeState resumeState; Book* epub;
 explicit EpubReaderActivity(Book* b): epub(b) {} void rememberBookOnceRendered(); };
struct TxtReaderActivity {
 ReaderResumeState resumeState; Book* txt;
 explicit TxtReaderActivity(Book* b): txt(b) {}
 bool initialized = true, loadOk = true; int currentPage = 0, totalPages = 1, renders = 0, progressWrites = 0;
 Renderer renderer; MappedInput mappedInput;
 std::vector<size_t> pageOffsets{0}; std::vector<std::string> currentPageLines;
 void initializeReader() { initialized = true; }
 bool loadPageAtOffset(size_t, std::vector<std::string>&, size_t&) { return loadOk; }
 void renderPage() { ++renders; }
 void saveProgress(bool) { ++progressWrites; }
 void render(RenderLock&&);
 void rememberBookOnceRendered();
};
struct XtcReaderActivity { ReaderResumeState resumeState; Book* xtc;
 explicit XtcReaderActivity(Book* b): xtc(b) {} void rememberBookOnceRendered(); };
''' + methods + function((reader / 'TxtReaderActivity.cpp').read_text(encoding='utf-8'), 'void TxtReaderActivity::render(') + r'''
template<class T> void scenario(const char* path, uint64_t id) {
 Book book; book.path = path;
 T activity{&book};
 APP_STATE = {}; APP_STATE.openEpubPath = "/previous.epub";
 APP_STATE.lastSleepFromReader = true; APP_STATE.readerActivityLoadCount = 1;
 RECENT_BOOKS = {};
 ReaderResumeState::clearRememberedBook();
 assert(APP_STATE.openEpubPath.empty() && APP_STATE.writes == 1);
 assert(APP_STATE.lastSleepFromReader && APP_STATE.readerActivityLoadCount == 1);
 ReaderResumeState::clearRememberedBook(); assert(APP_STATE.writes == 1);
 // No success signal: neither error display nor early exit remembers the book.
 for (int i = 0; i < 50; ++i) activity.rememberBookOnceRendered();
 assert(APP_STATE.openEpubPath.empty() && RECENT_BOOKS.writes == 0);
 // Render-task completion, immediately followed by main-task exit/sleep.
 std::thread render([&] { activity.resumeState.markPageRendered(); }); render.join();
 activity.rememberBookOnceRendered();
 assert(APP_STATE.openEpubPath == path && APP_STATE.writes == 2);
 assert(RECENT_BOOKS.path == path && RECENT_BOOKS.bookId == id && RECENT_BOOKS.writes == 1);
 for (int i = 0; i < 50; ++i) {
  activity.resumeState.markPageRendered(); activity.rememberBookOnceRendered();
 }
 assert(APP_STATE.writes == 2 && RECENT_BOOKS.writes == 1);
}
int main() {
 Book txtBook;
 // Execute the actual TXT render control flow: empty/error displays cannot
 // publish resume eligibility or write progress as though a page succeeded.
 for (int mode = 0; mode < 6; ++mode) {
  txtBook.fileSize = 100; txtBook.contentOffset = 0;
  TxtReaderActivity txt{&txtBook};
  if (mode == 0) txt.pageOffsets.clear();
  if (mode == 1) txt.loadOk = false;
  if (mode >= 3) {
   txt.initialized = false;
   txtBook.fileSize = mode == 3 ? 0 : mode == 4 ? 3 : 2;
   txtBook.contentOffset = mode == 3 ? 0 : 3;
  }
  txt.render(RenderLock{});
  assert(txt.resumeState.takeRenderedBook() == (mode == 2));
  assert(txt.renders == (mode == 2) && txt.progressWrites == (mode == 2));
  if (mode >= 3) assert(!txt.initialized);
 }
 for (int i = 0; i < 100; ++i) {
  scenario<EpubReaderActivity>("/fixture.epub", 0xFEDCBA9876543210ULL);
  scenario<TxtReaderActivity>("/fixture.txt", 0);
  scenario<XtcReaderActivity>("/fixture.xtc", 0);
 }
 Book book;
 EpubReaderActivity missing{nullptr}; missing.resumeState.markPageRendered();
 APP_STATE = {}; RECENT_BOOKS = {}; missing.rememberBookOnceRendered();
 assert(APP_STATE.writes == 0 && RECENT_BOOKS.writes == 0);
 std::cout << "PASS: 300 reader lifecycle scenarios and 6 production TXT render paths; empty/BOM-only files avoid indexing; immediate exit, failure, once-only writes and EPUB BookID\n";
}
'''
with tempfile.TemporaryDirectory(prefix='yomuka-reader-resume-') as directory:
    path = Path(directory)
    (path / 'CrossPointState.h').write_text('''#pragma once
#include <string>
struct State { std::string openEpubPath; bool lastSleepFromReader = false;
 int readerActivityLoadCount = 0, writes = 0; void saveToFile() { ++writes; } };
inline State APP_STATE;
''', encoding='utf-8')
    cpp = path / 'test.cpp'
    cpp.write_text(source, encoding='utf-8')
    exe = path / 'test'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-pthread', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I'+str(path), '-I'+str(reader), str(cpp), str(reader / 'ReaderResumeState.cpp'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
