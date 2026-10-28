import json
from pathlib import Path
from shutil import copyfile
import numpy as np
from PIL import Image

root = Path('C:/dev/midnafx')
out = root/'docs/evidence/color-bounce-2026-10-28'
for trial_name in ['native-r1','native-r2']:
    trial = root/'build/color-bounce'/trial_name
    target = out/trial_name
    target.mkdir(exist_ok=True)
    for source, name in [('session.json','session.json'), ('exit.json','exit.json'),
                         ('user/config.json','final-config.json'), ('stderr.log','stderr.log')]:
        copyfile(trial/source, target/name)
    lines = (trial/'stdout.log').read_text(errors='replace').splitlines()
    selected = [line for line in lines if any(s in line for s in [
        'com.midnafx.midnafx', 'all mods unloaded', 'WARNING', '[ERROR'])]
    (target/'lifecycle.log').write_text('\n'.join(selected)+'\n')

def read(name):
    return np.asarray(Image.open(out/name).convert('RGB')).astype(int)

comparisons = {}
for name, a, b in [
    ('simulation_stopped_candidate','final-off.jpg','final-on-candidate.jpg'),
    ('camera_and_actor_lock','locked-off.jpg','locked-on.jpg'),
    ('ao_held_active','shared-off.jpg','shared-on.jpg'),
    ('unchanged_ao_control','shared-off.jpg','shared-off-repeat.jpg')]:
    before, after = read(a), read(b)
    delta = np.max(np.abs(after-before),axis=2)
    crop = delta[40:600,260:700]
    comparisons[name] = {'before':a,'after':b,
        'world_roi_xyxy':[260,40,700,600], 'changed_pixels':int((crop>0).sum()),
        'pixels_over_3_codes':int((crop>3).sum()),'max_channel_delta':int(crop.max()),
        'screen_mean_rgb_delta':(after[40:690]-before[40:690]).mean(axis=(0,1)).tolist()}
(out/'native-comparisons.json').write_text(json.dumps(comparisons,indent=2)+'\n')
copyfile(root/'build/color-bounce-final-tests.log',out/'focused-tests.log')
copyfile(root/'build/color-bounce-ambient-tests.log',out/'ambient-pixels.log')
