import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]; out=r/'build/chapter_incremental';out.mkdir(parents=True,exist_ok=True)
s=(r/'lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp').read_text(encoding='utf-8')
start=s.index('ChapterHtmlSlimParser::~ChapterHtmlSlimParser()');end=s.index('void ChapterHtmlSlimParser::completeCurrentPage()',start)
(out/'production_driver.inc').write_text(s[start:end],encoding='utf-8')
objects=[]
for name in ['xmlparse','xmlrole','xmltok']:
 obj=out/(name+'.o');objects.append(str(obj))
 subprocess.run([a.compiler,'cc','-DXML_STATIC','-DXML_GE=1','-DXML_CONTEXT_BYTES=1024','-DHAVE_EXPAT_CONFIG_H','-I'+str(r/'lib/expat'),'-c',str(r/'lib/expat'/(name+'.c')),'-o',str(obj)],check=True)
exe=out/'test.exe'
subprocess.run([a.compiler,'c++','-std=c++20','-DXML_STATIC','-I'+str(r/'lib/expat'),'-I'+str(out),str(r/'test/chapter_incremental/DriverTest.cpp'),*objects,'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

budget=out/'budget.exe'
subprocess.run([a.compiler,'c++','-std=c++20','-I'+str(r/'lib/Epub'),str(r/'test/chapter_incremental/BudgetTest.cpp'),'-o',str(budget)],check=True)
subprocess.run([str(budget)],check=True)
print('PASS: conditional one-shot font recovery; sufficient, total-low, fragmented, missing-font cases')

recovery=out/'png_draw_recovery.exe'
subprocess.run([a.compiler,'c++','-std=c++20','-I'+str(r/'lib/Epub'),str(r/'test/chapter_incremental/PngDrawRecoveryTest.cpp'),'-o',str(recovery)],check=True)
subprocess.run([str(recovery)],check=True)
print('PASS: PNG draw-only conditional recovery; healthy/boundary/fragmented/total-low/cache-only/no-font')
