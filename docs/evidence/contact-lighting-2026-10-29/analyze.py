"""Assert retained baseline/candidate GPU results and native lifecycle receipts."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
baseline = (root / "baseline-gpu.log").read_text(encoding="utf-8")
candidate = (root / "candidate-gpu.log").read_text(encoding="utf-8")
before_cases = [line for line in baseline.splitlines()
                if line.endswith((": PASS", ": FAIL")) and "ambient pixel cases:" not in line]
after_cases = [line for line in candidate.splitlines()
               if line.endswith((": PASS", ": FAIL")) and "ambient pixel cases:" not in line]
assert len(before_cases) == len(after_cases) == 60
assert sum(line.endswith(": FAIL") for line in before_cases) == 12
assert all(line.endswith(": PASS") for line in after_cases)
assert all(line.endswith(": PASS") for line in before_cases if not line.startswith("Steep plane"))
old_normals = re.findall(r"Steep plane edge normal .*dot=(-?[\d.]+): FAIL", baseline)
assert len(old_normals) == 8
assert candidate.count("dot=1.000000: PASS") == 8

native = root / "native-compare"
receipt = lambda name: json.loads((native / name).read_text(encoding="utf-8-sig"))
assert receipt("exit.json")["exit_code"] == 0
log = (native / "native.log").read_text(encoding="utf-8")
assert log.count("reloading from") == 1
assert "all mods unloaded" in log
for effect in ("Ambient occlusion active:", "Contact shadows active:", "Color bounce active:"):
    assert log.count(effect) == 2
shutdowns = [tuple(map(int, row)) for row in re.findall(
    r"Contact shading shutdown: compute=(\d+) draw=(\d+) target_sets=(\d+)", log)]
assert len(shutdowns) == 2
assert all(compute > 0 and compute == draw and targets == 1 for compute, draw, targets in shutdowns)
before_config, after_config = receipt("config-before.json"), receipt("config-after.json")
for key in ("ambient_occlusion", "contact_shadows", "color_bounce", "motion_clarity"):
    full_key = "mod.com_midnafx_midnafx." + key
    assert before_config[full_key] is True and after_config[full_key] is True

summary = {
    "gpu_baseline_source": "v1.15.0",
    "baseline_assertions": 60,
    "baseline_failures": 12,
    "candidate_assertions": 60,
    "candidate_failures": 0,
    "analytic_normal_fixtures": 8,
    "old_normal_dots": list(map(float, old_normals)),
    "candidate_normal_dots": [1.0] * 8,
    "session": receipt("session.json"),
    "reload": receipt("reload.json"),
    "native_shutdowns": [dict(zip(("compute", "draw", "target_sets"), row)) for row in shutdowns],
    "native_exit_code": 0,
    "native_pixel_parity": "not established; simulation resumed for queued reload",
}
(root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print("GPU regression, candidate geometry and native lifecycle assertions pass.")
