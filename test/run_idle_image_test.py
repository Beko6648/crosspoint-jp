import argparse, subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
out=r/'build/idle_image_test_host';out.mkdir(parents=True,exist_ok=True)
s=(r/'lib/hal/HalGPIO.cpp').read_text(encoding='utf-8')
s=s[s.index('void HalGPIO::update() {'):s.index('bool HalGPIO::wasUsbStateChanged() const')]
(out/'HalGPIOIdleMethods.inc').write_text(s,encoding='utf-8')
cmd=[a.compiler,'c++','-std=c++17','-g','-O0','-DIDLE_IMAGE_PREFETCH_TEST=1']
cmd += ['-I'+str(x) for x in [out,r/'test/image_cache/stubs',r/'lib/Epub/Epub/converters']]
exe=out/'handoff.exe'
subprocess.run(cmd+[str(r/'test/idle_image/InputHandoffTest.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('Production HAL handoff: press retained, release delivered once, USB retained; cancellation latched, partial cache removed, retry succeeds')

# Both libraries may return before a full image; cancellation must win over success/finalize.
for name, call in [('Jpeg','jpeg->decode('),('Png','png->decode(')]:
    source=(r/f'lib/Epub/Epub/converters/{name}ToFramebufferConverter.cpp').read_text(encoding='utf-8')
    tail=source[source.index(call):]
    assert tail.index('config.cancellation->cancelled') < tail.index('ctx.cache.finalize()')
    cancelled=tail[tail.index('config.cancellation->cancelled'):tail.index('ctx.cache.finalize()')]
    assert 'ctx.cache.abort();' in cancelled and 'return false;' in cancelled
print('JPEG/PNG post-decode cancellation guards precede cache finalize')

exe=out/'png_budget.exe'
subprocess.run(cmd+[str(r/'test/idle_image/PngBudgetTest.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PNG admission: device samples, exact boundary, fragmentation, overflow passed')
