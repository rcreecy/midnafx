"""Analytic evidence fixtures only; these are not benchmark measurements."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('grass_cpu', Path(__file__).resolve().parents[1] / 'tools/grass-cpu.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


def fixture():
    return [dict(schema=1, sample=i, us=[10, 2, 3, 4, 5], roots=100, ready=True,
                 eye_xz=[1.0, 2.0], furthest=10000.0) for i in range(384)]


class Evidence(unittest.TestCase):
    def read(self, rows):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'stdout.log'
            path.write_text('\n'.join('Grass CPU: '+json.dumps(row) for row in rows))
            return bench.read_samples(path)

    def test_units_and_warmup(self):
        rows = self.read(fixture())
        result = bench.analyze(rows)
        self.assertEqual(result['ready_samples'], 256)
        self.assertEqual(result['phases_us']['reach']['p50'], 10)
        self.assertEqual(result['phases_us']['reach']['mean'], 10)
        self.assertEqual(result['selected_stage_us']['p99'], 24)

    def test_reject_corruption(self):
        for value in [float('nan'), float('inf'), -1, True, None]:
            rows = fixture()
            rows[0]['us'][0] = value
            with self.assertRaises(ValueError): self.read(rows)
        for length in [0, 383, 385]:
            rows = fixture()
            if length == 385: rows.append(dict(rows[-1], sample=384))
            with self.assertRaises(ValueError): self.read(rows[:length])
        rows = fixture(); rows[1]['sample'] = True
        with self.assertRaises(ValueError): self.read(rows)
        rows = fixture(); rows[0], rows[1] = rows[1], rows[0]
        with self.assertRaises(ValueError): self.read(rows)

    def test_insufficient_ready_samples(self):
        rows = fixture()
        for row in rows: row['ready'] = False
        with self.assertRaises(ValueError): bench.analyze(rows)
        with self.assertRaises(ValueError): bench.analyze(fixture(), -1)

    def test_camera_match_and_float_parity(self):
        before, after = fixture(), fixture()
        check = bench.compare_inputs(before, after)
        self.assertEqual(check['matched_camera_xz_count'], 1)
        self.assertEqual(check['furthest_mismatched_inputs'], [])
        after[-1]['furthest'] = 10001
        self.assertEqual(len(bench.compare_inputs(before, after)['furthest_mismatched_inputs']), 1)
        for row in after: row['eye_xz'] = [2, 3]
        self.assertEqual(bench.compare_inputs(before, after)['matched_camera_xz_count'], 0)

    def test_geometry_proofs_require_matching_inputs(self):
        before, after = fixture(), fixture()
        self.assertEqual(bench.compare_geometry(before, after)['matched_input_hash_count'], 0)
        for rows in (before, after):
            rows[128].update(input_hash='a'*16, geometry_hash='b'*16)
        after[128]['assembly_reused'] = True
        self.assertEqual(bench.compare_geometry(before, after)['candidate_reused_proof_samples'], 1)
        self.assertEqual(bench.compare_geometry(before, after)['geometry_mismatched_inputs'], [])
        after[128]['geometry_hash'] = 'c'*16
        self.assertEqual(bench.compare_geometry(before, after)['geometry_mismatched_inputs'], ['a'*16])
        after[128]['input_hash'] = 'd'*16
        self.assertEqual(bench.compare_geometry(before, after)['matched_input_hash_count'], 0)

    def test_immutable_upload_evidence(self):
        before, after = fixture(), fixture()
        for row in before: row.update(uploaded_bytes=6000, resident_bytes=0, immutable_upload=False)
        for row in after: row.update(uploaded_bytes=0, resident_bytes=6000, immutable_upload=True)
        summary = bench.analyze(self.read(after))
        self.assertEqual(summary['immutable_samples'], 256)
        self.assertEqual(summary['streamed_instance_bytes'], 0)
        self.assertEqual(summary['max_owned_instance_bytes'], 6000)
        for rows in (before, after): rows[383].update(input_hash='a'*16, instance_hash='b'*16)
        self.assertEqual(bench.compare_immutable_instances(before, after)['immutable_proofs_checked'], 1)
        self.assertEqual(bench.compare_immutable_instances(before, after)['instance_hash_mismatched_inputs'], [])
        after[383]['instance_hash'] = 'c'*16
        self.assertEqual(bench.compare_immutable_instances(before, after)['instance_hash_mismatched_inputs'], ['a'*16])
        after[0]['uploaded_bytes'] = -1
        with self.assertRaises(ValueError): self.read(after)

    def test_airborne_span_and_ordered_proof(self):
        before, after = fixture(), fixture()
        for rows in (before, after):
            for row in rows: row.update(discovery_us=10, airborne_us=100, airborne_count=6, airborne_sampled=True, airborne_reused=False)
            rows[383].update(input_hash='a'*16, airborne_hash='b'*16)
        for row in after: row.update(airborne_us=1, airborne_reused=True)
        result = bench.analyze(self.read(after), 288)
        self.assertEqual(result['airborne_reused_samples'], 96)
        self.assertEqual(result['external_spans_us']['airborne']['p50'], 1)
        self.assertEqual(bench.compare_airborne(before, after)['reused_anchor_proofs_checked'], 1)
        after[383]['airborne_hash'] = 'c'*16
        self.assertEqual(bench.compare_airborne(before, after)['ordered_anchor_mismatches'], ['a'*16])
        after[0]['airborne_us'] = float('nan')
        with self.assertRaises(ValueError): self.read(after)


if __name__ == '__main__':
    unittest.main()
