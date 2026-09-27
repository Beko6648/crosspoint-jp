import argparse, subprocess, shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
for baseline in (True,False):
 out=r/'build/button_navigation'/('baseline' if baseline else 'candidate');out.mkdir(parents=True,exist_ok=True)
 for name in ('ButtonNavigator.h','ButtonNavigator.cpp'):
  rel='src/util/'+name
  data=subprocess.check_output(['git','show','e9a5989a:'+rel],cwd=r) if baseline else (r/rel).read_bytes()
  (out/name).write_bytes(data)
 shutil.copyfile(r/'test/button_navigation/MappedInputManager.h',out/'MappedInputManager.h')
 exe=out/'test.exe'
 cmd=[a.compiler,'c++','-std=c++20','-I'+str(out),str(r/'test/button_navigation/NavigationTest.cpp'),str(out/'ButtonNavigator.cpp'),'-o',str(exe)]
 if baseline: cmd.append('-DBASELINE')
 subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
