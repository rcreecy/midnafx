"""Assert controlled player-light phases, current source data and lifecycle."""
import csv
import json
import math
import re
import struct
from collections import Counter
from pathlib import Path

root=Path(__file__).resolve().parent
native=root/"native-final"
rows=list(csv.DictReader((native/"observations.csv").open(encoding="utf-8")))
phases=Counter(int(row["phase"]) for row in rows)
assert set(phases)=={0,1,2,3}
assert phases[1]==150 and phases[2]==60 and phases[3]==120
brightness=struct.unpack("f",struct.pack("f",0.99999))[0]
for row in rows:
    phase=int(row["phase"])
    active=int(row["active"])
    assert int(row["injected"])==active==int(phase in (1,3))
    ref=float(row["reference_distance"])
    coefficient=float(row["k2"])
    envelope=float(row["envelope"])
    if active:
        assert 0.25<=ref<=1
        expected=(1-brightness)/(ref*(brightness*ref))
        assert math.isclose(coefficient,expected,rel_tol=2e-6)
        assert math.isclose(envelope,math.sqrt(99/coefficient),rel_tol=2e-6)
    else:
        assert coefficient==envelope==0
        assert ref>0 # Inactive native fields still contain stale nonzero values.
moving=[row for row in rows if int(row["phase"])==1]
assert max(float(r["x"]) for r in moving)-min(float(r["x"]) for r in moving)>100
assert max(float(r["reference_distance"]) for r in moving)-min(float(r["reference_distance"]) for r in moving)>0.7
gpu=(root/"gpu-final.log").read_text(encoding="utf-8")
assert "87 ambient pixel cases: PASS" in gpu and ": FAIL" not in gpu
assert gpu.count("Native steep curve at half/fifth intensity distances")==2
log=(native/"native.log").read_text(encoding="utf-8")
assert log.count("Player local light active:")==2
assert log.count("reloading from")==1
assert "Player-light audit phases complete" in log and "all mods unloaded" in log
restores,conflicts=map(int,re.search(r"Player-light audit retired: restores=(\d+) conflicts=(\d+)",log).groups())
assert restores>0 and conflicts==0
shutdowns=[tuple(map(int,row)) for row in re.findall(r"Contact shading shutdown: compute=(\d+) draw=(\d+) target_sets=(\d+)",log)]
assert len(shutdowns)==2 and all(compute==draw and compute>0 and targets==1 for compute,draw,targets in shutdowns)
assert sum(row[0] for row in shutdowns)==restores
read_json=lambda name: json.loads((native/name).read_text(encoding="utf-8-sig"))
assert read_json("exit.json")["exit_code"]==0
config=read_json("config-after.json")
assert config["mod.com_midnafx_midnafx.local_lights"] is True
assert config["mod.com_midnafx_midnafx.local_lights_strength"]==15
summary={"gpu_cases":87,"recorded_source_ticks":len(rows),"phase_counts":dict(phases),
    "source_restorations":restores,"ownership_conflicts":conflicts,"exit_code":0,
    "shutdowns":[dict(zip(("compute","draw","target_sets"),row)) for row in shutdowns],
    "session":read_json("session.json"),"equipped_lantern_gameplay":"not tested"}
(root/"summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
print("Lantern coefficients, moving/fading/inactive source phases, ownership and native lifecycle pass.")
