#!/usr/bin/env python3
"""Compile/run the allocation-free snapshot progress helpers on a host.

No downloads or non-stdlib Python modules. Requires GCC/Clang with C++17.
This does NOT build firmware, run the existing JSON-schema tests, or test SD I/O.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"),
                        help="Single compiler executable/path, not a shell command")
    parser.add_argument("--sanitize", action="store_true", help="Enable AddressSanitizer and UndefinedBehaviorSanitizer")
    parser.add_argument("--json-report", type=Path, help="Write result and exact command to this JSON file")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is None:
        parser.error(f"Compiler not found: {args.compiler}. Install GCC/Clang or pass --compiler.")
    root = Path(__file__).resolve().parents[1]
    sources = [root / "src/sync/SyncProgress.cpp", root / "test/sync_progress_foundation/test_sync_progress.cpp"]
    for source in sources:
        if not source.is_file():
            parser.error(f"Source missing: {source}")
    report: dict[str, object] = {"compiler": compiler, "sanitize": args.sanitize, "passed": False}
    try:
        version = subprocess.run([compiler, "--version"], text=True, capture_output=True, check=True, timeout=15)
        report["compiler_version"] = version.stdout.splitlines()[0]
        with tempfile.TemporaryDirectory(prefix="yomuka-sync-progress-") as temporary:
            executable = Path(temporary) / ("sync_progress_tests.exe" if os.name == "nt" else "sync_progress_tests")
            flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wsign-conversion", "-Werror",
                     "-fno-exceptions", "-fno-rtti"]
            flags += ["-O1", "-g", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"] if args.sanitize else ["-O2"]
            command = [compiler, *flags, "-I", str(root / "src/sync"), *map(str, sources), "-o", str(executable)]
            report["command"] = command
            result = subprocess.run(command, text=True, capture_output=True, timeout=120)
            report["compile_stdout"] = result.stdout
            report["compile_stderr"] = result.stderr
            if result.returncode != 0:
                sys.stderr.write(result.stdout + result.stderr)
                raise RuntimeError(f"Compile failed (exit {result.returncode})")
            environment = dict(os.environ)
            if args.sanitize:
                environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
                environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            result = subprocess.run([str(executable)], text=True, capture_output=True, env=environment, timeout=60)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            report["test_stdout"] = result.stdout
            report["test_stderr"] = result.stderr
            if result.returncode != 0:
                raise RuntimeError(f"Tests failed (exit {result.returncode})")
            summary = re.search(r"SUMMARY cases=(\d+) checks=(\d+)", result.stdout)
            if summary is None:
                raise RuntimeError("Test process did not emit its summary")
            report.update(passed=True, cases=int(summary[1]), checks=int(summary[2]))
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        report["error"] = str(error)
        print(f"ERROR: {error}", file=sys.stderr)
    if args.json_report is not None:
        args.json_report.parent.mkdir(parents=True, exist_ok=True)
        args.json_report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
