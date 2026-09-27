"""Exercise the production control class with deterministic input and clock doubles."""
from pathlib import Path
import subprocess, tempfile, argparse, re
p=argparse.ArgumentParser(); p.add_argument('--compiler',required=True); a=p.parse_args()
root=Path(__file__).resolve().parents[1]
source=(root/'src/util/CacheGenerationControls.h').read_text()
source=re.sub(r'^#(?:include|pragma).*\n','',source,flags=re.M)
stubs=r'''
#include <cassert>
#include <cstdint>
#define LOG_INF(...) ((void)0)
#define LOW 0
uint32_t now=0; bool power=false, down=false;
uint32_t millis(){return now;}
int digitalRead(int){return power?0:1;}
int analogRead(int){return down?0:4095;}
struct InputManager {static constexpr int POWER_BUTTON_PIN=3, BUTTON_ADC_PIN_2=2;};
struct GfxRenderer {};
struct GPIO {void update(){}} gpio;
struct MappedInputManager {enum class Button{Back}; bool back=false; bool isPressed(Button)const{return back;}};
struct ScreenshotUtil {inline static int shots=0; static void takeScreenshot(GfxRenderer&){++shots;}};
'''
cases=r'''
int main(){
GfxRenderer r; MappedInputManager i;
CacheGenerationControls c(i);
// Starting with Back held must first require release.
i.back=true; assert(!c.shouldCancel(r)); now=2000; assert(!c.shouldCancel(r));
i.back=false; assert(!c.shouldCancel(r));
i.back=true; now=2100; assert(!c.shouldCancel(r)); now=2500; assert(!c.shouldCancel(r));
i.back=false; assert(!c.shouldCancel(r));
i.back=true; now=2600; assert(!c.shouldCancel(r)); now=3599; assert(!c.shouldCancel(r));
now=3600; assert(c.shouldCancel(r)); assert(c.shouldCancel(r));
assert(CacheGenerationControls::consumeCancellationRelease(i));
i.back=false; assert(CacheGenerationControls::consumeCancellationRelease(i));
assert(!CacheGenerationControls::consumeCancellationRelease(i));
// Screenshot chord wins and discards a partly held cancellation.
CacheGenerationControls d(i); assert(!d.shouldCancel(r)); i.back=true; now=4000; assert(!d.shouldCancel(r));
power=down=true; now=6000; assert(!d.shouldCancel(r)); assert(!d.shouldCancel(r)); assert(ScreenshotUtil::shots==1);
power=down=false; assert(!d.shouldCancel(r)); i.back=false; assert(!d.shouldCancel(r));
// Timer must work across millis wraparound.
i.back=true; now=0xfffffe00u; assert(!d.shouldCancel(r)); now+=1000; assert(d.shouldCancel(r));
i.back=false; assert(CacheGenerationControls::consumeCancellationRelease(i));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 f=Path(tmp)/'controls.cpp'; exe=Path(tmp)/'controls.exe'; f.write_text(stubs+source+cases)
 subprocess.run([a.compiler,'c++','-std=c++17',str(f),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: initial held key, short press, long hold, latch, release consumption, screenshot chord and timer wrap')
