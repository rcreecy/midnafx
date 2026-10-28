import csv, json, pathlib, shutil
root = pathlib.Path('C:/dev/midnafx')
out = root/'docs/evidence/motion-clarity-2026-10-28'
summary = {}
for name, expected in [('native-on-final',0),('native-off-final',160),('native-half-final',80)]:
    source = root/'build/motion-clarity'/name
    lines = (source/'audit.csv').read_text().splitlines()
    assert '# complete' in lines, name
    rows = list(csv.DictReader(line for line in lines if line and not line.startswith('#')))
    for row in rows:
        for field in ['tick','rate','enabled','history','event','paused','fading']:
            row[field] = int(row[field])
    gameplay = [r for r in rows if r['stage']=='opaque' and r['history'] and
                not r['event'] and not r['paused'] and not r['fading']]
    events = [r for r in rows if r['stage']=='opaque' and r['event'] and r['history']]
    begun = {r['tick'] for r in rows if r['stage']=='begin'}
    hud = [r for r in rows if r['stage']=='hud' and r['tick'] in begun]
    startup_hud = [r for r in rows if r['stage']=='hud' and r['tick'] not in begun]
    assert len(gameplay)>30 and len(events)>30 and len(hud)>100, name
    assert all(r['rate']==expected for r in gameplay), (name,'gameplay')
    assert all(r['rate']==160 for r in events), (name,'events')
    assert all(r['rate']==160 for r in hud), (name,'restoration')
    assert all(r['enabled']==1 for r in gameplay+events+hud), (name,'native enable flag')
    summary[name] = {'gameplay_frames':len(gameplay),'gameplay_alpha':expected,
        'event_frames':len(events),'event_alpha':160,'restored_hud_frames':len(hud),'hud_alpha':160,
        'native_enable_preserved':True,'complete':True,'hud_without_fixture_request':len(startup_hud)}
    target = out/name
    target.mkdir(exist_ok=True)
    for src,dest in [('audit.csv','audit.csv'),('session.json','session.json'),
                     ('user/config.json','final-config.json'),('exit.json','exit.json'),('stderr.log','stderr.log')]:
        if (source/src).exists(): shutil.copyfile(source/src,target/dest)
    if (source/'stdout.log').exists():
        log = (source/'stdout.log').read_text(errors='replace').splitlines()
        selected = [line for line in log if any(s in line for s in ['Motion clarity', 'all mods unloaded',
            'activating mod','deactivating mod','WARNING','[ERROR'])]
        (target/'lifecycle.log').write_text('\n'.join(selected)+'\n')
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['driver/driver.cpp','driver/CMakeLists.txt','launch-native.ps1']:
    shutil.copyfile(root/'build/motion-clarity'/name,out/('driver-CMakeLists.txt' if name.endswith('CMakeLists.txt') else pathlib.Path(name).name))
shutil.copyfile(root/'build/motion-clarity-final-tests.log',out/'focused-tests.log')
print(json.dumps(summary,indent=2))
