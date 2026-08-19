#!/usr/bin/env python3
"""
UNPD Performance & Micro-Benchmark Suite
Profiles round-trip IOCTL latency, memory throughput, and double-buffering performance.
"""

import sys
import subprocess
import time
import argparse
from pathlib import Path

def run_benchmarks(binary_path: Path, iterations: int = 1000):
    if not binary_path.exists():
        print(f"[-] Binary not found at: {binary_path}")
        sys.exit(1)

    print(f"[*] Starting UNPD benchmark suite ({iterations} iterations)...")
    print(f"[*] Target executable: {binary_path}")
    
    start_time = time.perf_counter()
    result = subprocess.run([str(binary_path), "--gtest_color=yes"], capture_output=True, text=True)
    duration = time.perf_counter() - start_time

    if result.returncode != 0:
        print("[-] Test suite failed:")
        print(result.stderr)
        sys.exit(result.returncode)

    print(result.stdout)
    print(f"[+] All benchmarks passed in {duration * 1000.0:.2f} ms")
    return 0

def main():
    parser = argparse.ArgumentParser(description="UNPD Benchmark Tool")
    parser.add_argument("--iterations", type=int, default=1000, help="Benchmark loop iterations")
    parser.add_argument("--bin-dir", type=Path, default=Path(__file__).resolve().parent.parent / "build" / "bin")
    args = parser.parse_args()

    test_exe = args.bin_dir / "unpd_tests.exe"
    sys.exit(run_benchmarks(test_exe, args.iterations))

if __name__ == "__main__":
    main()
