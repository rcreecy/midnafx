"""Validate evidence handling independently of hardware performance."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('performance_bench', Path(__file__).resolve().parents[1] / 'tools/performance-bench.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        # Analytic fixture, not captured timing evidence.
        self.raw = dict(schema=1, units='cpu_microseconds_per_24300_queries', workloads=[
            dict(scenario='fixture', index_bytes=100, build_us=10, trials=[
                dict(order='AB' if i % 2 == 0 else 'BA', baseline_us=2000, optimized_us=1000)
                for i in range(6)])])

    def test_units_and_paired_ratios(self):
        item = bench.summarize(self.raw)[0]
        self.assertEqual(item['baseline_ms']['p50'], 2)
        self.assertEqual(item['optimized_ms']['p99'], 1)
        self.assertEqual(item['paired_ratio_median'], .5)

    def test_reject_invalid_evidence(self):
        for value in [0, -1, float('nan'), float('inf'), True, None]:
            raw = copy.deepcopy(self.raw)
            raw['workloads'][0]['trials'][0]['optimized_us'] = value
            with self.assertRaises(ValueError):
                bench.summarize(raw)
        self.raw['units'] = 'gpu_ms'
        with self.assertRaises(ValueError):
            bench.summarize(self.raw)

    def test_requires_paired_orders_and_repeats(self):
        for trials in [self.raw['workloads'][0]['trials'][:2],
                       [dict(order='AB', baseline_us=10, optimized_us=5)] * 6]:
            raw = copy.deepcopy(self.raw)
            raw['workloads'][0]['trials'] = trials
            with self.assertRaises(ValueError):
                bench.summarize(raw)

    def test_empty_capture_is_not_success(self):
        self.raw['workloads'] = []
        with self.assertRaises(ValueError):
            bench.summarize(self.raw)


if __name__ == '__main__':
    unittest.main()
