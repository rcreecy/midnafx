"""Check process-counter arithmetic and reject unmatched qualification evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
spec = importlib.util.spec_from_file_location('overhead', ROOT / 'tools/asset-overhead.py')
overhead = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overhead)


def fixture():
    runs = []
    for variant_index, variant in enumerate(overhead.VARIANTS):
        for trial in range(3):
            name = f'{variant}-{trial}'
            config = {'video.enableVsync': True, 'backend.isoPath': 'private/local/disc.rvz'}
            config.update({key: variant == 'patched-on' for key in overhead.FLAGS})
            session = dict(name=name, variant=variant, config=config,
                           host_sha256=('a' if variant_index == 0 else 'b') * 64,
                           package_sha256='c' * 64)
            capture = dict(name=name, variant=variant, warmup_after_visual_check_seconds=20,
                           samples=[dict(seconds=i, cpu_seconds=10 + i * (variant_index + 1) / 2,
                                         private_bytes=(100 + trial + variant_index * 10) * 2**20,
                                         working_set_bytes=(90 + trial + variant_index * 10) * 2**20)
                                    for i in range(31)])
            runs.append((session, capture))
    return runs


class OverheadTests(unittest.TestCase):
    def test_units_group_medians_ranges_and_deltas(self):
        result = overhead.analyze(fixture())
        baseline = result['groups']['baseline']
        self.assertEqual(baseline['cpu_ms_per_wall_second'], dict(median=500, min=500, max=500))
        self.assertEqual(baseline['median_private_mib'], dict(median=101, min=100, max=102))
        self.assertEqual(result['groups']['patched-on']['cpu_ms_per_wall_second']['median'], 1500)
        self.assertEqual(result['median_deltas']['host_patch']['median_private_mib'], 10)
        self.assertEqual(result['median_deltas']['enabled_probe']['cpu_ms_per_wall_second'], 500)
        self.assertNotIn('backend.isoPath', result['controls'])

    def test_counters_must_be_finite_and_monotonic(self):
        for key, value in [('seconds', 0), ('cpu_seconds', 0), ('private_bytes', 0),
                           ('working_set_bytes', float('nan')), ('cpu_seconds', float('inf'))]:
            with self.subTest(key=key, value=value):
                runs = fixture()
                runs[0][1]['samples'][1][key] = value
                with self.assertRaises(ValueError):
                    overhead.analyze(runs)

    def test_reject_short_or_incomplete_capture(self):
        for mutation in ('warmup', 'count', 'duration'):
            with self.subTest(mutation=mutation):
                runs = fixture()
                capture = runs[0][1]
                if mutation == 'warmup':
                    capture['warmup_after_visual_check_seconds'] = 19
                elif mutation == 'count':
                    capture['samples'].pop()
                else:
                    for row in capture['samples']:
                        row['seconds'] *= 0.5
                with self.assertRaises(ValueError):
                    overhead.analyze(runs)

    def test_reject_unmatched_controls_and_packages(self):
        for field in ('config', 'package_sha256'):
            with self.subTest(field=field):
                runs = fixture()
                if field == 'config':
                    runs[0][0]['config']['video.enableVsync'] = False
                else:
                    runs[0][0][field] = 'd' * 64
                with self.assertRaises(ValueError):
                    overhead.analyze(runs)

    def test_reject_wrong_flags_and_run_identity(self):
        for mutation in ('flag', 'name', 'variant'):
            with self.subTest(mutation=mutation):
                runs = fixture()
                if mutation == 'flag':
                    runs[0][0]['config'][overhead.FLAGS[0]] = True
                else:
                    runs[0][1][mutation] = 'wrong'
                with self.assertRaises(ValueError):
                    overhead.analyze(runs)

    def test_reject_unmatched_host_binaries(self):
        for indices in ([0], range(3, 6), range(6, 9)):
            with self.subTest(indices=indices):
                runs = fixture()
                for index in indices:
                    runs[index][0]['host_sha256'] = 'a' * 64 if index >= 3 else 'd' * 64
                with self.assertRaises(ValueError):
                    overhead.analyze(runs)

    def test_reject_missing_trials_or_duplicate_names(self):
        runs = fixture()
        with self.assertRaises(ValueError):
            overhead.analyze(runs[:-1])
        with self.assertRaises(ValueError):
            overhead.analyze(runs + [copy.deepcopy(runs[0])])

    def test_recorded_evidence_reproduces_summary(self):
        root = ROOT / 'docs/evidence/asset-overhead-2026-10-21'
        sessions = json.loads((root / 'runs.json').read_text())
        inputs = [(run['session'], run['capture']) for run in sessions]
        summary = json.loads((root / 'summary.json').read_text())
        self.assertEqual(overhead.analyze(inputs), summary['analysis'])
        self.assertEqual(sum(len(run['log_observations']['events']['AssetDraw']) for run in sessions),
                         summary['protocol']['active_draw_labels'])
        for run in sessions:
            observed = run['log_observations']
            self.assertTrue(observed['normal_shutdown'])
            self.assertTrue(observed['adapter_verified'])
            events = observed['events']['AssetGraph']
            if run['session']['variant'] == 'patched-on':
                self.assertEqual([event['event'] for event in events], ['draw-cost'])
                self.assertGreater(events[0]['callbacks'], 0)
            else:
                self.assertEqual(events, [])


if __name__ == '__main__':
    unittest.main()
