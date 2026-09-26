import argparse
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--compiler", required=True, help="Path to zig executable")
args = parser.parse_args()
out = root / "build/heap-trace-output-test.exe"
out.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([args.compiler, "c++", "-std=c++17", "-I" + str(root / "src/util"),
                str(root / "test/heap_trace_output/OutputTest.cpp"), "-o", str(out)], check=True)
subprocess.run([str(out)], check=True)
print("Output readiness and frame write: recovery, timeout, clock wrap, stalled/full queue kick, no partial retry passed")
