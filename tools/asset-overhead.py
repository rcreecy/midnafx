"""Analyze process counters; no FPS, allocation-rate or GPU claims.

Counter arithmetic alone does not qualify a workload. New Windows studies must
also use --require-host-identity and verify the exercised asset paths in logs.
"""
import argparse
import json
from pathlib import Path
import statistics

VARIANTS=('baseline','patched-off','patched-on')
FLAGS=('mod.com_midnafx_midnafx.asset_diagnostics','mod.com_midnafx_midnafx.asset_draw_diagnostics')

def validate_host_identity(session):
    """Require the Windows import name and one matching loaded host image."""
    if session.get('host_executable_name', '').casefold() != 'dusklight.exe':
        raise ValueError('profiling requires executable name dusklight.exe')
    images=session.get('host_images', [])
    if (len(images)!=1 or images[0].get('name', '').casefold()!='dusklight.exe' or
            images[0].get('sha256', '').casefold()!=session['host_sha256'].casefold()):
        raise ValueError('profiling requires one matching loaded host image')

def summarize_run(session, capture):
    variant=session['variant']
    if variant not in VARIANTS or capture['variant']!=variant or capture['name']!=session['name']:
        raise ValueError('run identity mismatch')
    if capture['warmup_after_visual_check_seconds']<20:
        raise ValueError('insufficient warmup')
    if any(session['config'].get(key) is not (variant=='patched-on') for key in FLAGS):
        raise ValueError('diagnostic flags differ from variant')
    samples=capture['samples']
    if len(samples)!=31:
        raise ValueError('expected 30 intervals')
    for i,row in enumerate(samples):
        if any(type(row[k]) not in (int,float) or not 0<=row[k]<float('inf') for k in ('seconds','cpu_seconds','private_bytes','working_set_bytes')):
            raise ValueError('invalid process counter')
        if not row['private_bytes'] or not row['working_set_bytes']:
            raise ValueError('missing memory counter')
        if i and (row['seconds']<=samples[i-1]['seconds'] or row['cpu_seconds']<samples[i-1]['cpu_seconds']):
            raise ValueError('nonmonotonic process counters')
    elapsed=samples[-1]['seconds']-samples[0]['seconds']
    if not 29<=elapsed<=40:
        raise ValueError('measurement duration outside qualification window')
    return dict(name=session['name'],variant=variant,host_sha256=session['host_sha256'],package_sha256=session['package_sha256'],
                measured_seconds=elapsed,cpu_ms_per_wall_second=1000*(samples[-1]['cpu_seconds']-samples[0]['cpu_seconds'])/elapsed,
                median_private_mib=statistics.median(s['private_bytes'] for s in samples)/2**20,
                median_working_set_mib=statistics.median(s['working_set_bytes'] for s in samples)/2**20,
                private_range_mib=[min(s['private_bytes'] for s in samples)/2**20,max(s['private_bytes'] for s in samples)/2**20])

def analyze(inputs):
    if len({session['name'] for session,_ in inputs})!=len(inputs):
        raise ValueError('duplicate run name')
    controls=[{k:v for k,v in s['config'].items() if k not in FLAGS} for s,_ in inputs]
    if not controls or any(c!=controls[0] for c in controls):
        raise ValueError('unmatched controls')
    runs=[summarize_run(s,c) for s,c in inputs]
    if len({r['package_sha256'] for r in runs})!=1:
        raise ValueError('unmatched mod package')
    groups={}
    for variant in VARIANTS:
        selected=[r for r in runs if r['variant']==variant]
        if len(selected)<3 or len({r['host_sha256'] for r in selected})!=1:
            raise ValueError('need three runs per variant with one host binary')
        groups[variant]={'runs':len(selected),'host_sha256':selected[0]['host_sha256']}
        for key in ('cpu_ms_per_wall_second','median_private_mib','median_working_set_mib'):
            values=[r[key] for r in selected]
            groups[variant][key]={'median':statistics.median(values),'min':min(values),'max':max(values)}
    if groups['patched-off']['host_sha256']!=groups['patched-on']['host_sha256'] or groups['baseline']['host_sha256']==groups['patched-off']['host_sha256']:
        raise ValueError('baseline and diagnostic host identities are invalid')
    deltas={}
    for name,left,right in (('host_patch','baseline','patched-off'),('enabled_probe','patched-off','patched-on')):
        deltas[name]={k:groups[right][k]['median']-groups[left][k]['median'] for k in ('cpu_ms_per_wall_second','median_private_mib','median_working_set_mib')}
    return dict(schema=1,measurement='steady process CPU and retained memory; inspect companion logs for exercised diagnostic paths',
                controls={k:v for k,v in controls[0].items() if k!='backend.isoPath'},runs=runs,groups=groups,median_deltas=deltas,
                limitations=['three trials are observations, not statistical confidence intervals',
                             'dynamic scene and host scheduling remain uncontrolled',
                             'no FPS, GPU, allocation-rate or active resample-burst measurement'])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directories',nargs='+',type=Path)
    parser.add_argument('--require-host-identity',action='store_true',
                        help='Reject missing module proof, renamed hosts and duplicate host images')
    args=parser.parse_args()
    try:
        inputs=[(json.loads((p/'session.json').read_text(encoding='utf-8-sig')),
                 json.loads((p/'process-samples.json').read_text(encoding='utf-8-sig'))) for p in args.run_directories]
        if args.require_host_identity:
            for session,capture in inputs:
                validate_host_identity(session)
                for key in ('host_images_before','host_images_after'):
                    validate_host_identity(dict(session,host_images=capture.get(key,[])))
                if capture.get('visual_check_confirmed') is not True:
                    raise ValueError('profiling requires a confirmed visual scene check')
        print(json.dumps(analyze(inputs),sort_keys=True,indent=2))
    except (ValueError,KeyError,OSError,TypeError) as error:
        parser.exit(1,str(error)+'\n')

if __name__=='__main__': main()
