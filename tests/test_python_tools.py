#!/usr/bin/env python3
"""
Unit and Integration Tests for UNPD Python Automation & Fuzzing Tools
"""

import unittest
import sys
from pathlib import Path

# Add scripts/python directory to path
SCRIPTS_DIR = Path(__file__).resolve().parent.parent / "scripts" / "python"
sys.path.insert(0, str(SCRIPTS_DIR))

from bench_latency import LatencyStats
from fuzz_runner import FuzzGenerator, run_fuzzing
from verify_pe import PEInspector
from update_language_stats import scan_repository, generate_markdown

class TestLatencyStats(unittest.TestCase):
    def test_empty_samples(self):
        stats = LatencyStats([])
        self.assertEqual(stats.mean(), 0.0)
        self.assertEqual(stats.median(), 0.0)
        self.assertEqual(stats.p95(), 0.0)
        self.assertEqual(stats.p99(), 0.0)

    def test_single_sample(self):
        stats = LatencyStats([42.0])
        self.assertEqual(stats.mean(), 42.0)
        self.assertEqual(stats.median(), 42.0)
        self.assertEqual(stats.p95(), 42.0)
        self.assertEqual(stats.p99(), 42.0)

    def test_percentile_calculation(self):
        samples = list(range(1, 101))  # 1 to 100
        stats = LatencyStats(samples)
        self.assertAlmostEqual(stats.median(), 50.5, places=1)
        self.assertAlmostEqual(stats.p95(), 95.05, delta=1.0)
        self.assertAlmostEqual(stats.p99(), 99.01, delta=1.0)

class TestFuzzGenerator(unittest.TestCase):
    def test_boundary_sizes(self):
        sizes = FuzzGenerator.boundary_sizes()
        self.assertIn(0, sizes)
        self.assertIn(4096, sizes)
        self.assertIn(0xFFFFFFFF, sizes)

    def test_mutation_generation(self):
        mutations = FuzzGenerator.generate_mutations()
        self.assertGreater(len(mutations), 10)
        for ioctl, payload, desc in mutations:
            self.assertIsInstance(ioctl, int)
            self.assertIsInstance(payload, bytes)
            self.assertIsInstance(desc, str)

    def test_fuzz_runner_cycles(self):
        rc = run_fuzzing(100)
        self.assertEqual(rc, 0)

class TestLanguageStats(unittest.TestCase):
    def test_scan_repository(self):
        stats = scan_repository()
        self.assertIn("C++", stats)
        self.assertIn("Lua", stats)
        self.assertIn("Python", stats)
        self.assertGreater(stats["Lua"]["files"], 0)
        self.assertGreater(stats["C++"]["bytes"], 1000)

        md = generate_markdown(stats)
        self.assertIn("UNPD Repository Language Metrics", md)
        self.assertIn("Lua", md)

if __name__ == "__main__":
    unittest.main()
