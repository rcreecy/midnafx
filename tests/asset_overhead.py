"""Check process-counter arithmetic and reject unmatched qualification evidence."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
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
    @unittest.skipUnless(os.name == 'nt', 'Windows sampler contract')
    def test_sampler_requires_visual_acknowledgement_before_reading_files(self):
        shell=shutil.which('pwsh') or shutil.which('powershell')
        self.assertIsNotNone(shell)
        with tempfile.TemporaryDirectory() as folder:
            directory=Path(folder)/'absent'
            result=subprocess.run([shell,'-NoProfile','-NonInteractive','-File',
                                   str(ROOT/'tools/asset-overhead-sample.ps1'),'-RunDirectory',str(directory)],
                                  capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('visual scene check is required',result.stdout+result.stderr)
            self.assertFalse(directory.exists())

    def test_strict_cli_accepts_verified_windows_utf8_bom_sessions(self):
        with tempfile.TemporaryDirectory() as folder:
            directories=[]
            for session,capture in fixture():
                session['host_executable_name']='dusklight.exe'
                session['host_images']=[dict(name='dusklight.exe',sha256=session['host_sha256'])]
                capture['host_images_before']=copy.deepcopy(session['host_images'])
                capture['host_images_after']=copy.deepcopy(session['host_images'])
                capture['visual_check_confirmed']=True
                session['config']['label']='caf\u00e9'
                directory=Path(folder)/session['name']
                directory.mkdir()
                (directory/'session.json').write_text(json.dumps(session,ensure_ascii=False),encoding='utf-8-sig')
                (directory/'process-samples.json').write_text(json.dumps(capture),encoding='utf-8-sig')
                directories.append(str(directory))
            result=subprocess.run([sys.executable,str(ROOT/'tools/asset-overhead.py'),
                                   '--require-host-identity',*directories],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(json.loads(result.stdout)['controls']['label'],'caf\u00e9')
            first=Path(directories[0])/'process-samples.json'
            capture=json.loads(first.read_text(encoding='utf-8-sig'))
            capture['host_images_after'].append(dict(name='dusklight-alias.exe',sha256='a'*64))
            first.write_text(json.dumps(capture),encoding='utf-8-sig')
            rejected=subprocess.run([sys.executable,str(ROOT/'tools/asset-overhead.py'),
                                     '--require-host-identity',*directories],capture_output=True,text=True)
            self.assertNotEqual(rejected.returncode,0)
            self.assertIn('one matching loaded host image',rejected.stderr)

    def test_host_identity_requires_import_name_and_one_matching_image(self):
        valid=dict(host_executable_name='dusklight.exe', host_sha256='b'*64,
                   host_images=[dict(name='dusklight.exe', sha256='b'*64)])
        overhead.validate_host_identity(valid)
        for mutation in ('alias', 'duplicate', 'missing', 'hash'):
            with self.subTest(mutation=mutation):
                session=copy.deepcopy(valid)
                if mutation=='alias':
                    session['host_executable_name']='dusklight-asset-traced.exe'
                elif mutation=='duplicate':
                    session['host_images'].append(dict(name='dusklight-asset-traced.exe',sha256='b'*64))
                elif mutation=='missing':
                    session.pop('host_images')
                else:
                    session['host_images'][0]['sha256']='c'*64
                with self.assertRaises(ValueError):
                    overhead.validate_host_identity(session)

    @unittest.skipUnless(os.name == 'nt', 'Windows launcher contract')
    def test_launcher_rejects_host_alias_before_any_files_or_processes(self):
        shell = shutil.which('pwsh') or shutil.which('powershell')
        self.assertIsNotNone(shell)
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'must-not-exist'
            result = subprocess.run([shell, '-NoProfile', '-NonInteractive', '-File',
                                     str(ROOT / 'tools/asset-overhead-launch.ps1'),
                                     '-Name', 'rejected', '-Variant', 'patched-on',
                                     '-HostExecutable', str(Path(folder) / 'dusklight-asset-traced.exe'),
                                     '-ModPackage', 'absent.dusk', '-DiscPath', 'absent.rvz',
                                     '-ReplacementRoot', 'absent-pack', '-OutputRoot', str(output)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('requires executable name dusklight.exe', result.stdout + result.stderr)
            self.assertFalse(output.exists())

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
        self.assertFalse(summary['qualification']['valid'])
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

    def test_recorded_host_name_proof(self):
        root=ROOT / 'docs/evidence/asset-host-name-2026-10-21'
        proof=json.loads((root/'summary.json').read_text())
        normal,alias=proof['runs']
        self.assertEqual(normal['session']['config'],alias['session']['config'])
        for key in ('host_sha256','package_sha256'):
            self.assertEqual(normal['session'][key],alias['session'][key])
        overhead.validate_host_identity(normal['session'])
        with self.assertRaises(ValueError):
            overhead.validate_host_identity(alias['session'])
        self.assertEqual(len(alias['session']['host_images']),2)
        for run in proof['runs']:
            self.assertTrue(run['startup_confirmed'])
            events=[json.loads(line) for line in (root/f"{run['role']}-events.jsonl").read_text().splitlines()]
            labels=[e['data'] for e in events if e['kind']=='AssetGraph' and e['data']['event']=='draw-label']
            draws=[e['data'] for e in events if e['kind']=='AssetDraw']
            self.assertEqual(len(labels),run['counts']['draw_labels'])
            self.assertEqual(len(draws),run['counts']['encoded_draws'])
            self.assertEqual({e['label'] for e in labels},{e['label'] for e in draws})
            if run['role']=='normal':
                self.assertEqual(len(labels),32)
                self.assertEqual(len({e['label'] for e in labels}),32)
                self.assertEqual({e['material'] for e in labels},{0,1})
                self.assertEqual({e['id'] for e in labels},{'pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421'})
                self.assertTrue(all(e['instances']>0 and (e['vertices']>0 or e['indices']>0) for e in draws))
            else:
                self.assertTrue(all(event['kind']=='AssetProvider' for event in events))
                self.assertEqual(len(labels),0)
                self.assertEqual(len(draws),0)

    def test_corrected_recording_has_populated_workload_and_verified_hosts(self):
        root=ROOT/'docs/evidence/asset-overhead-corrected-2026-10-21'
        runs=json.loads((root/'runs.json').read_text())
        summary=json.loads((root/'summary.json').read_text())
        self.assertEqual(overhead.analyze([(r['session'],r['capture']) for r in runs]),summary['analysis'])
        events=[json.loads(line) for line in (root/'metadata-events.jsonl').read_text().splitlines()]
        forced=[]
        for run in runs:
            session,capture=run['session'],run['capture']
            overhead.validate_host_identity(session)
            self.assertTrue(capture['visual_check_confirmed'])
            for key in ('host_images_before','host_images_after'):
                overhead.validate_host_identity(dict(session,host_images=capture[key]))
            selected=[e for e in events if e['run']==session['name']]
            labels=[e['data'] for e in selected if e['kind']=='AssetGraph' and e['data']['event']=='draw-label']
            draws=[e['data'] for e in selected if e['kind']=='AssetDraw']
            self.assertEqual(len(labels),run['log_observations']['counts']['draw_labels'])
            self.assertEqual(len(draws),run['log_observations']['counts']['encoded_draws'])
            if session['variant']=='patched-on':
                self.assertEqual(len(labels),32)
                self.assertEqual(len(draws),32)
                self.assertEqual({e['label'] for e in labels},{e['label'] for e in draws})
                self.assertEqual(len({e['label'] for e in labels}),32)
                self.assertEqual({e['material'] for e in labels},{0,1})
                self.assertGreater(run['log_observations']['counts']['graph']['resource'],0)
            else:
                self.assertEqual(labels,[])
                self.assertEqual(draws,[])
                self.assertEqual(run['log_observations']['counts']['graph'],{})
            self.assertTrue(run['log_observations']['mod_unload_confirmed'])
            if not run['log_observations']['normal_shutdown']:
                forced.append(session['name'])
                self.assertIsNotNone(run['log_observations']['termination'])
        self.assertEqual(forced,['b2'])
        self.assertFalse(summary['full_overhead_gate_closed'])
        self.assertEqual(summary['macos_gameplay']['owner'],'user')


if __name__ == '__main__':
    unittest.main()
