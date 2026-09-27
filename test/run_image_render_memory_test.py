import argparse, subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(); p.add_argument('--compiler',required=True); a=p.parse_args()
out=r/'build/image_render_memory'; out.mkdir(parents=True,exist_ok=True)
for source, flags in [('TraceTest', ['-DIMAGE_RENDER_MEMORY_DIAGNOSTICS=1', '-I'+str(r/'test/image_render_memory/stubs')]), ('DisabledTest', []), ('BudgetTest', [])]:
    exe=out/(source+'.exe')
    subprocess.run([a.compiler,'c++','-std=c++17','-O0','-I'+str(r/'lib/GfxRenderer'),*flags,str(r/'test/image_render_memory'/(source+'.cpp')),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Deferred snapshots: capture values, task filter, nested scope, text suppression, bounded overflow; disabled build has no platform dependency: passed')
