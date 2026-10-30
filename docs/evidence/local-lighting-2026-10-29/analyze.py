"""Check retained GPU, configuration and native local-light observations."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
gpu = (root / "gpu-final.log").read_text(encoding="utf-8")
assert "79 ambient pixel cases: PASS" in gpu
assert ": FAIL" not in gpu
lifted = list(map(int,re.findall(r"Default 15% local light .*lifted=(\d+): PASS",gpu)))
blocked = list(map(int,re.findall(r"Visible opaque caster .*count=(\d+): PASS",gpu)))
assert lifted == [608,241] and blocked == [60,5]
assert "Zero local strength preserves bounce output and fields exactly despite poisoned sources: PASS" in gpu

native = root / "native-final"
read_json = lambda name: json.loads((native / name).read_text(encoding="utf-8-sig"))
assert read_json("exit.json")["exit_code"] == 0
log = (native / "native.log").read_text(encoding="utf-8")
assert log.count("Local light response active: sources=1 strength=0.15") == 2
assert log.count("reloading from") == 1
assert log.count("Audit ordinary light: index=2") == 1
assert "Audit ordinary light retired" in log and "all mods unloaded" in log
assert "Contact lighting compute unavailable" not in log
assert "Contact lighting targets unavailable" not in log
shutdowns = [tuple(map(int,row)) for row in re.findall(
    r"Contact shading shutdown: compute=(\d+) draw=(\d+) target_sets=(\d+)",log)]
assert len(shutdowns) == 2
assert all(compute > 0 and compute == draw and targets == 1 for compute,draw,targets in shutdowns)
config = read_json("config-after.json")
assert config["mod.com_midnafx_midnafx.local_lights"] is True
assert config["mod.com_midnafx_midnafx.local_lights_strength"] == 15
for key in ("ambient_occlusion","contact_shadows","color_bounce","grading_enabled","sunlight_shafts","bloom"):
    assert config["mod.com_midnafx_midnafx."+key] is False
summary = {
    "gpu_cases":79,
    "default_strength":0.15,
    "gpu_lifted_pixels":lifted,
    "gpu_occluded_receivers":blocked,
    "native_source_count":1,
    "session":read_json("session.json"),
    "final_reload":read_json("reload.json"),
    "native_shutdowns":[dict(zip(("compute","draw","target_sets"),row)) for row in shutdowns],
    "exit_code":0,
    "native_quality":"context only; no frozen image comparison or natural torch matrix",
}
(root / "summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
print("Local-light GPU, independent control, source retirement and native reload/close assertions pass.")
