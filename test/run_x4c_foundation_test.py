#!/usr/bin/env python3
"""Check production power logging and X4C diagnostic serialization on a host."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
hardware = root / "freeink-sdk/libs/hardware"
compiler = os.environ.get("CXX", "c++")
with tempfile.TemporaryDirectory(prefix="yomuka-x4c-") as directory:
    out = Path(directory)
    source = (root / "src/main.cpp").read_text(encoding="utf-8")
    start = source.index("static void appendPowerLog(")
    end = source.index("// Enter deep sleep mode", start)
    (out / "PowerLog.inc").write_text(source[start:end], encoding="utf-8")
    for x4c in (0, 1):
        exe = out / f"power-{x4c}"
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        f"-DFREEINK_DEVICE_X4CLASSIC={x4c}", "-I" + str(out),
                        str(root / "test/x4c_foundation/PowerLogTest.cpp"), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    exe = out / "report"
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-DFREEINK_DEVICE_X4CLASSIC=1", "-I" + str(root / "src"),
                    "-I" + str(hardware / "XteinkDetect/test/host/stubs"),
                    "-I" + str(hardware / "BoardConfig/include"),
                    "-I" + str(hardware / "XteinkDetect/include"),
                    str(root / "test/x4c_foundation/ReportTest.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
