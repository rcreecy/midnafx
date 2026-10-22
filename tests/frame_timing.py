"""Check timing semantics, evidence integrity, and asynchronous frame ordering."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('frame_timing', ROOT / 'tools/frame-timing.py')
TIMING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TIMING)


def fixture():
    origin = 1_000_000_000
    producer, present = [], []
    for index in range(601):
        begin = origin + index * 100_000_000
        producer.append(dict(frame_id=index + 1, begin_entry_ns=begin,
                             acquisition_complete_ns=begin + 1_000_000,
                             record_start_ns=begin + 2_000_000, end_entry_ns=begin + 12_000_000,
                             end_enqueue_complete_ns=begin + 30_000_000))
        # Real render work can finish before the producer returns from enqueue.
        present.append(dict(frame_id=index + 1, completion_ns=begin + 20_000_000,
                            interval_ns=100_000_000 if index else None))
    return dict(schema=1, clock='std::chrono::steady_clock', unit='ns',
                semantics=dict(encoding_time=None, gpu_time=None, heap_allocations=None),
                capacity_per_stream=1024, producer_count=len(producer), producer_dropped=0,
                present_count=len(present), present_dropped=0, begin_attempts=len(producer),
                failed_begins=0, incomplete_begins=0, truncated=False, producer=producer, present=present)


class FrameTimingTests(unittest.TestCase):
    def test_arithmetic_and_present_before_enqueue_return(self):
        result = TIMING.analyze(fixture())
        for metric, expected in (('acquisition_wall', 1), ('producer_record_wall', 10),
                                 ('end_enqueue_wall', 18), ('total_producer_wall', 30),
                                 ('consecutive_begin_interval', 100),
                                 ('successful_present_completion_interval', 100)):
            self.assertEqual(result['metrics'][metric]['p50'], expected)
            self.assertEqual(result['metrics'][metric]['p99'], expected)
        self.assertEqual(result['selected']['complete_producer_frames'], 300)
        self.assertEqual(result['metrics']['consecutive_begin_interval']['count'], 300)
        self.assertEqual(result['metrics']['successful_present_completion_interval']['count'], 299)
        for key in TIMING.MISSING_METRICS:
            self.assertIsNone(result[key])

    def test_percentiles_use_linear_interpolation(self):
        stats = TIMING.distribution([1_000_000, 2_000_000, 3_000_000, 10_000_000])
        self.assertEqual(stats['p50'], 2.5)
        self.assertAlmostEqual(stats['p95'], 8.95)
        self.assertAlmostEqual(stats['p99'], 9.79)
        self.assertEqual(stats['min'], 1)
        self.assertEqual(stats['max'], 10)

    def test_failed_and_incomplete_begins_have_no_fabricated_rows(self):
        capture = fixture()
        capture.update(begin_attempts=605, failed_begins=3, incomplete_begins=1)
        result = TIMING.analyze(capture)
        self.assertEqual(result['source']['failed_begins'], 3)
        self.assertEqual(result['source']['incomplete_begins'], 1)

    def test_missing_successful_presents_are_allowed(self):
        capture = fixture()
        capture['present'] = capture['present'][::2]
        for index, row in enumerate(capture['present']):
            row['interval_ns'] = 200_000_000 if index else None
        capture['present_count'] = len(capture['present'])
        self.assertEqual(TIMING.analyze(capture)['metrics']['successful_present_completion_interval']['p50'], 200)

    def test_rejects_reordered_rows_and_timestamps(self):
        for mutation in ('producer_id', 'producer_start', 'producer_overlap', 'local_order',
                         'present_id', 'present_time', 'present_interval'):
            with self.subTest(mutation=mutation):
                capture = fixture()
                if mutation == 'producer_id': capture['producer'][1]['frame_id'] = 1
                elif mutation == 'producer_start': capture['producer'][1]['begin_entry_ns'] = 1
                elif mutation == 'producer_overlap': capture['producer'][0]['end_enqueue_complete_ns'] += 100_000_000
                elif mutation == 'local_order': capture['producer'][2]['record_start_ns'] -= 50_000_000
                elif mutation == 'present_id': capture['present'][1]['frame_id'] = 1
                elif mutation == 'present_time': capture['present'][1]['completion_ns'] = capture['present'][0]['completion_ns']
                elif mutation == 'present_interval': capture['present'][1]['interval_ns'] = 1
                with self.assertRaises(ValueError): TIMING.analyze(capture)

    def test_rejects_malformed_metadata_and_counter_equations(self):
        for key, value in (('producer_count', 600), ('present_count', 600), ('producer_dropped', 1),
                           ('capacity_per_stream', 600), ('begin_attempts', 602), ('failed_begins', -1),
                           ('incomplete_begins', True), ('truncated', True), ('schema', True), ('unit', 'ms')):
            with self.subTest(key=key):
                capture = fixture()
                capture[key] = value
                with self.assertRaises(ValueError): TIMING.analyze(capture)

    def test_rejects_truncated_capture_even_if_window_precedes_drops(self):
        capture = fixture()
        capture.update(capacity_per_stream=601, producer_dropped=1, begin_attempts=602, truncated=True)
        with self.assertRaisesRegex(ValueError, 'truncated source'): TIMING.analyze(capture)

    def test_rejects_insufficient_or_empty_coverage(self):
        for start, duration in ((40, 30), (60, 1), (20, .001)):
            with self.subTest(start=start, duration=duration):
                with self.assertRaisesRegex(ValueError, 'insufficient'):
                    TIMING.analyze(fixture(), start, duration)
        capture = fixture()
        capture.update(present=[], present_count=0)
        with self.assertRaisesRegex(ValueError, 'coverage'): TIMING.analyze(capture)

    def test_rejects_missing_or_invented_metrics_and_invalid_scalars(self):
        for mutation in ('missing_metric', 'invented_metric', 'timestamp_bool', 'timestamp_float',
                         'timestamp_overflow', 'first_interval', 'missing_interval', 'nonobject_row'):
            with self.subTest(mutation=mutation):
                capture = fixture()
                if mutation == 'missing_metric': del capture['semantics']['gpu_time']
                elif mutation == 'invented_metric': capture['semantics']['gpu_time'] = 4
                elif mutation == 'timestamp_bool': capture['producer'][0]['begin_entry_ns'] = True
                elif mutation == 'timestamp_float': capture['producer'][0]['begin_entry_ns'] = 1.0
                elif mutation == 'timestamp_overflow': capture['producer'][0]['begin_entry_ns'] = -(2**64)
                elif mutation == 'first_interval': capture['present'][0]['interval_ns'] = 0
                elif mutation == 'missing_interval': del capture['present'][0]['interval_ns']
                elif mutation == 'nonobject_row': capture['producer'][0] = None
                with self.assertRaises(ValueError): TIMING.analyze(capture)

    def test_invalid_window_arguments(self):
        for start, duration in ((-1, 30), (0, 0), (0, -1), (float('nan'), 1), (0, float('inf')),
                                (True, 30), (10**1000, 30), (0, 1e308), (0, 1e-20)):
            with self.subTest(start=start, duration=duration):
                with self.assertRaises(ValueError): TIMING.analyze(fixture(), start, duration)

    def test_signed_clock_epoch_and_equal_local_timestamps_are_valid(self):
        capture = fixture()
        for row in capture['producer']:
            for key in TIMING.PRODUCER_TIMES:
                row[key] -= 100_000_000_000
            row['acquisition_complete_ns'] = row['begin_entry_ns']
        for row in capture['present']:
            row['completion_ns'] -= 100_000_000_000
        self.assertEqual(TIMING.analyze(capture)['metrics']['acquisition_wall']['max'], 0)

    def test_cli_accepts_bom_and_writes_optional_output(self):
        with tempfile.TemporaryDirectory() as folder:
            source, output = Path(folder) / 'capture.json', Path(folder) / 'summary.json'
            source.write_text(json.dumps(fixture()), encoding='utf-8-sig')
            command = [sys.executable, str(ROOT / 'tools/frame-timing.py'), '--input', str(source)]
            trial = subprocess.run(command + ['--output', str(output)], capture_output=True, text=True)
            self.assertEqual(trial.returncode, 0, trial.stderr)
            self.assertEqual(json.loads(output.read_text())['metrics']['acquisition_wall']['p50'], 1)
            rejected = subprocess.run(command + ['--start-seconds', '40'], capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 1)
            self.assertIn('insufficient coverage', rejected.stderr)


if __name__ == '__main__':
    unittest.main()
