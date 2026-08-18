#!/usr/bin/env python3
"""
UNPD Kernel Latency & Throughput Benchmark Script
Measures IOCTL ping roundtrip latency, memory page map speed, and double-buffer swap throughput.
"""

import sys
import subprocess
import time
import json
from pathlib import Path

def run_tests(binary_path: Path):
    if not binary_path.exists():
        print(f"[!] Error: Binary {binary_path} not found.")
        sys.exit(1)

    print(f"[*] Running GoogleTest suite: {binary_path}")
    start = time.perf_counter()
    result = subprocess.run([str(binary_path), "--gtest_color=yes"], capture_output=True, text=True)
    elapsed = time.perf_counter() - start

    print(result.stdout)
    if result.returncode != 0:
        print(result.stderr)
        print(f"[!] Test run failed with return code {result.returncode}")
        sys.exit(result.returncode)

    print(f"[+] All tests completed in {elapsed * 1000.0:.2f} ms")

if __name__ == "__main__":
    bin_dir = Path(__file__).resolve().parent.parent / "build" / "bin"
    test_exe = bin_dir / "unpd_tests.exe"
    run_tests(test_exe)
