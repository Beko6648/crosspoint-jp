import argparse
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument("--compiler", required=True)
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
out = r / "build/single_page_cache"
out.mkdir(parents=True, exist_ok=True)
exe = out / "test.exe"
subprocess.run([a.compiler, "c++", "-std=c++20", "-I" + str(r / "lib/Epub"), str(r / "test/single_page_cache/CompletionTest.cpp"), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
# Integration guards: source media must be observed before hidden/unsupported skips,
# and no proof is published before persisted-section validation/rename succeeds.
s = (r / "lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp").read_text(encoding="utf-8")
s = s[s.index("void XMLCALL ChapterHtmlSlimParser::startElement"):]
assert s.index("isMediaElement(name)") < s.index("skipUntilDepth")
s = (r / "lib/Epub/Epub/Section.cpp").read_text(encoding="utf-8")
after = s[s.index("bool textOnlySource = false;"):]
assert after.index("!finalizeSectionFile(") < after.index("freshTextOnlyBuild = textOnlySource && cssReady")
print("PASS: completion eligibility, media exclusion, and publication ordering")
