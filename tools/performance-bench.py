"""Run or summarize paired mod-local CPU benchmarks. Never an FPS/GPU timing gate."""
import argparse
import hashlib
import importlib.util
import json
import math
import platform
import statistics
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('frame_timing', ROOT / 'tools/frame-timing.py')
timing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timing)


def summarize(raw):
    if type(raw.get('schema')) is not int or raw.get('schema') != 1 or raw.get('units') != 'cpu_microseconds_per_24300_queries':
        raise ValueError('unknown benchmark schema or timing units')
    if not raw.get('workloads'):
        raise ValueError('empty benchmark capture')
    result = []
    for workload in raw['workloads']:
        trials = workload['trials']
        if len(trials) < 6 or {t['order'] for t in trials} != {'AB', 'BA'}:
            raise ValueError('need at least six paired trials in both orders')
        for trial in trials:
            for key in ('baseline_us', 'optimized_us'):
                value = trial[key]
                if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
                    raise ValueError('timings must be finite and positive')
        ratios = [t['optimized_us'] / t['baseline_us'] for t in trials]
        # Reuse established percentile convention; distribution accepts ns and returns ms.
        distributions = {key.removesuffix('_us') + '_ms': timing.distribution([t[key] * 1000 for t in trials])
                         for key in ('baseline_us', 'optimized_us')}
        result.append(dict(scenario=workload['scenario'], **distributions,
                           paired_ratio_median=statistics.median(ratios),
                           paired_ratio_min=min(ratios), paired_ratio_max=max(ratios),
                           index_bytes=workload['index_bytes'], build_us=workload['build_us']))
    return result


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--executable', type=Path)
    source.add_argument('--input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--hardware', type=Path, help='JSON metadata supplied by the operator')
    parser.add_argument('--reference', type=Path, help='Previous report on the same reference machine')
    args = parser.parse_args()
    if args.executable:
        # check=True retains failures as failures, never a successful empty capture.
        capture = subprocess.run([str(args.executable.resolve()), '--benchmark'], capture_output=True, text=True, check=True)
        raw = json.loads(capture.stdout)
        binary_hash = hashlib.sha256(args.executable.read_bytes()).hexdigest()
    else:
        loaded = json.loads(args.input.read_text(encoding='utf-8-sig'))
        raw = loaded.get('raw', loaded)
        binary_hash = None
    report = dict(schema=1, measured_at_utc=datetime.now(timezone.utc).isoformat(),
                  scope='synthetic mod-local CPU query batches, not game frame times',
                  head=git('rev-parse', 'HEAD'), status=git('status', '--short'),
                  tracked_diff_sha256=hashlib.sha256(subprocess.check_output(['git', '-C', str(ROOT), 'diff', 'HEAD'])).hexdigest(),
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in (ROOT / 'src/render/terrain_index.hpp', ROOT / 'src/render/ground_grass.cpp', ROOT / 'tests/terrain_index.cpp')},
                  executable_sha256=binary_hash, platform=platform.platform(), processor=platform.processor(),
                  analysis_input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest() if args.input else None,
                  metadata_scope='analysis environment; original capture metadata in input report' if args.input else 'capture environment',
                  hardware=json.loads(args.hardware.read_text(encoding='utf-8-sig')) if args.hardware else None,
                  gpu_time=None, whole_frame_percentiles=None,
                  summary=summarize(raw), raw=raw)
    if args.reference:
        reference = json.loads(args.reference.read_text(encoding='utf-8-sig'))
        if reference['hardware'] != report['hardware'] or reference['platform'] != report['platform']:
            raise ValueError('reference machine metadata differs')
        old = {s['scenario']: s for s in reference['summary']}
        report['trend'] = [dict(scenario=s['scenario'], optimized_p50_ratio=s['optimized_ms']['p50'] / old[s['scenario']]['optimized_ms']['p50'])
                           for s in report['summary']]
        # Informational only: repeated machine-specific evidence is required for a regression decision.
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    for item in report['summary']:
        print(f"{item['scenario']}: paired optimized/baseline median {item['paired_ratio_median']:.4f}, "
              f"range {item['paired_ratio_min']:.4f}..{item['paired_ratio_max']:.4f}")


if __name__ == '__main__':
    main()
