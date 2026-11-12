"""Analyze bounded native grass CPU spans. No GPU/FPS measurements are inferred."""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import statistics
import struct
import re

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('frame_timing', ROOT / 'tools/frame-timing.py')
timing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timing)
PHASES = ('reach', 'requests', 'tiles', 'assembly', 'submission')


def read_samples(path):
    samples = []
    for line in path.read_text(errors='replace').splitlines():
        if 'Grass CPU: ' not in line:
            continue
        row = json.loads(line.split('Grass CPU: ', 1)[1])
        if type(row.get('schema')) is not int or row['schema'] != 1 or type(row.get('sample')) is not int or row['sample'] != len(samples):
            raise ValueError('unknown schema or missing/reordered grass samples')
        if type(row.get('ready')) is not bool or type(row.get('roots')) is not int or row['roots'] < 0:
            raise ValueError('invalid readiness or roots')
        if 'assembly_reused' in row and type(row['assembly_reused']) is not bool:
            raise ValueError('invalid assembly reuse flag')
        if 'immutable_upload' in row and type(row['immutable_upload']) is not bool:
            raise ValueError('invalid immutable upload flag')
        for name in ('uploaded_bytes', 'resident_bytes'):
            if name in row and (type(row[name]) is not int or not 0 <= row[name] <= 2**64-1):
                raise ValueError('invalid upload/storage byte counter')
        for name in ('discovery_us', 'airborne_us'):
            if name in row and (type(row[name]) not in (int, float) or not math.isfinite(row[name]) or row[name] < 0):
                raise ValueError('invalid external CPU span')
        for name in ('airborne_sampled', 'airborne_reused'):
            if name in row and type(row[name]) is not bool: raise ValueError('invalid airborne capture flag')
        if 'airborne_count' in row and (type(row['airborne_count']) is not int or row['airborne_count'] < 0):
            raise ValueError('invalid airborne count')
        if not isinstance(row.get('us'), list) or len(row['us']) != len(PHASES):
            raise ValueError('incomplete CPU phase record')
        if any(type(v) not in (int, float) or not math.isfinite(v) or v < 0 for v in row['us']):
            raise ValueError('invalid CPU span')
        if 'eye_xz' in row or 'furthest' in row:
            if not isinstance(row.get('eye_xz'), list) or len(row['eye_xz']) != 2:
                raise ValueError('incomplete reach input record')
            if any(type(v) not in (int, float) or not math.isfinite(v) for v in [*row['eye_xz'], row.get('furthest')]):
                raise ValueError('invalid reach input or result')
        if any(name in row and (not isinstance(row[name], str) or not re.fullmatch('[0-9a-f]{16}', row[name]))
               for name in ('input_hash', 'geometry_hash', 'instance_hash', 'airborne_hash')):
            raise ValueError('invalid geometry proof hash')
        samples.append(row)
    if len(samples) != 384:
        raise ValueError(f'expected 384 native samples, got {len(samples)}')
    return samples


def analyze(samples, warmup=128):
    if type(warmup) is not int or not 0 <= warmup <= 320:
        raise ValueError('invalid warmup count')
    selected = [row for row in samples[warmup:] if row['ready']]
    if len(selected) < 64:
        raise ValueError('insufficient ready samples after warmup')
    def dist(values_us):
        ms = timing.distribution([value * 1000 for value in values_us])
        return {key: value if key == 'count' else value * 1000 for key, value in ms.items()} | {'mean': statistics.fmean(values_us)}
    external = {}
    if all('discovery_us' in row for row in selected): external['discovery'] = dist([row['discovery_us'] for row in selected])
    airborne = [row for row in selected if row.get('airborne_sampled')]
    if airborne: external['airborne'] = dist([row['airborne_us'] for row in airborne])
    return dict(samples=len(samples), warmup_samples=warmup, ready_samples=len(selected), external_spans_us=external,
                airborne_samples=len(airborne), airborne_reused_samples=sum(row['airborne_reused'] for row in airborne),
                airborne_count_min=min((row['airborne_count'] for row in airborne), default=None),
                airborne_count_max=max((row['airborne_count'] for row in airborne), default=None),
                phases_us={name: dist([row['us'][i] for row in selected]) for i, name in enumerate(PHASES)},
                selected_stage_us=dist([sum(row['us']) for row in selected]),
                roots_min=min(row['roots'] for row in selected), roots_max=max(row['roots'] for row in selected),
                assembly_reused_samples=sum(row['assembly_reused'] for row in selected) if all('assembly_reused' in row for row in selected) else None,
                immutable_samples=sum(row['immutable_upload'] for row in selected) if all('immutable_upload' in row for row in selected) else None,
                streamed_instance_bytes=sum(row['uploaded_bytes'] for row in selected) if all('uploaded_bytes' in row for row in selected) else None,
                max_owned_instance_bytes=max(row['resident_bytes'] for row in selected) if all('resident_bytes' in row for row in selected) else None)


def compare_inputs(before, after, warmup=128):
    def mapping(samples):
        result = {}
        for row in samples[warmup:]:
            if row['ready'] and 'eye_xz' in row:
                key = struct.pack('!ff', *row['eye_xz']).hex()
                result.setdefault(key, set()).add(struct.pack('!f', row['furthest']).hex())
        return result
    a, b = mapping(before), mapping(after)
    common = a.keys() & b.keys()
    mismatches = sorted(key for key in common if a[key] != b[key])
    return dict(matched_camera_xz_count=len(common), furthest_mismatched_inputs=mismatches,
                reference_distinct_camera_xz=len(a), candidate_distinct_camera_xz=len(b))


def compare_geometry(before, after):
    def proofs(samples):
        result = {}
        for row in samples:
            if row['ready'] and row.get('input_hash', '0'*16) != '0'*16:
                result.setdefault(row['input_hash'], set()).add((row['geometry_hash'], row['roots']))
        return result
    a, b = proofs(before), proofs(after)
    common = a.keys() & b.keys()
    return dict(matched_input_hash_count=len(common),
                geometry_mismatched_inputs=sorted(key for key in common if a[key] != b[key]),
                candidate_reused_proof_samples=sum(row.get('assembly_reused', False) and row.get('input_hash') in common
                                                    for row in after))


def compare_immutable_instances(before, after):
    reference = {}
    for row in before:
        if row.get('instance_hash', '0'*16) != '0'*16:
            reference.setdefault(row['input_hash'], set()).add(row['instance_hash'])
    checked, mismatches = 0, []
    for row in after:
        if row.get('immutable_upload') and row.get('instance_hash', '0'*16) != '0'*16 and row['input_hash'] in reference:
            checked += 1
            if row['instance_hash'] not in reference[row['input_hash']]: mismatches.append(row['input_hash'])
    return dict(immutable_proofs_checked=checked, instance_hash_mismatched_inputs=mismatches)


def compare_airborne(before, after):
    reference = {}
    for row in before:
        if row.get('airborne_sampled') and row.get('airborne_hash', '0'*16) != '0'*16:
            reference.setdefault(row['input_hash'], set()).add((row['airborne_count'], row['airborne_hash']))
    checked, mismatches = 0, []
    for row in after:
        if row.get('airborne_reused') and row.get('airborne_hash', '0'*16) != '0'*16 and row['input_hash'] in reference:
            checked += 1
            if (row['airborne_count'], row['airborne_hash']) not in reference[row['input_hash']]: mismatches.append(row['input_hash'])
    return dict(reused_anchor_proofs_checked=checked, ordered_anchor_mismatches=mismatches)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    runs = []
    captures = []
    for run in manifest['runs']:
        if run['forced'] or run['exit_code'] != 0:
            raise ValueError('failed native trial; retain raw logs and investigate')
        directory = Path(run['directory'])
        if not directory.is_absolute():
            directory = args.manifest.parent / directory
        elif not directory.exists():
            directory = args.manifest.parent / directory.name
        samples = read_samples(directory / 'stdout.log')
        captures.append(samples)
        runs.append(dict(variant=run['variant'], directory=str(directory),
                         **analyze(samples, manifest['warmup_samples'])))
    if not runs:
        raise ValueError('empty native run manifest')
    report = dict(schema=1, scope='native grass producer CPU spans, excludes discovery and GPU execution',
                  gpu_us=None, whole_frame_us=None, metadata=manifest, runs=runs)
    if len(runs) % 2 == 0:
        pairs = []
        checks = []
        geometry = []
        stage_ratios = []
        assembly_means = []
        stage_means = []
        immutable_checks = []
        airborne_checks, external_ratios = [], []
        for i in range(0, len(runs), 2):
            pair = {run['variant']: run for run in runs[i:i+2]}
            if set(pair) != {'A', 'B'}:
                raise ValueError('each native pair must contain reference and candidate')
            pairs.append({name: pair['B']['phases_us'][name]['p50'] / pair['A']['phases_us'][name]['p50']
                          for name in PHASES if pair['A']['phases_us'][name]['p50'] > 0})
            a = i if runs[i]['variant'] == 'A' else i+1
            b = i if runs[i]['variant'] == 'B' else i+1
            checks.append(compare_inputs(captures[a], captures[b], manifest['warmup_samples']))
            geometry.append(compare_geometry(captures[a], captures[b]))
            immutable_checks.append(compare_immutable_instances(captures[a], captures[b]))
            airborne_checks.append(compare_airborne(captures[a], captures[b]))
            external_ratios.append({name: pair['B']['external_spans_us'][name]['p50'] / value['p50']
                                    for name, value in pair['A']['external_spans_us'].items()
                                    if name in pair['B']['external_spans_us'] and value['p50'] > 0})
            stage_ratios.append(pair['B']['selected_stage_us']['p50'] / pair['A']['selected_stage_us']['p50'])
            assembly_means.append(pair['B']['phases_us']['assembly']['mean'] / pair['A']['phases_us']['assembly']['mean'])
            stage_means.append(pair['B']['selected_stage_us']['mean'] / pair['A']['selected_stage_us']['mean'])
        report['paired_phase_p50_ratios'] = pairs
        report['paired_reach_ratio_median'] = statistics.median(pair['reach'] for pair in pairs)
        report['paired_reach_input_checks'] = checks
        report['paired_geometry_checks'] = geometry
        report['paired_immutable_instance_checks'] = immutable_checks
        report['paired_airborne_checks'] = airborne_checks
        report['paired_external_p50_ratios'] = external_ratios
        report['paired_assembly_ratio_median'] = statistics.median(pair['assembly'] for pair in pairs)
        report['paired_selected_stage_p50_ratios'] = stage_ratios
        report['paired_assembly_mean_ratios'] = assembly_means
        report['paired_selected_stage_mean_ratios'] = stage_means
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print(json.dumps([dict(variant=run['variant'], ready=run['ready_samples'],
                           p50_us={name: values['p50'] for name, values in run['phases_us'].items()}) for run in runs], indent=2))


if __name__ == '__main__':
    main()
