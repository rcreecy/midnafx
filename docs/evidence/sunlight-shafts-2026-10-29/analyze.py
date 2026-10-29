"""Assert native sunlight input qualification without claiming GPU pixel attribution."""
import csv
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parent
summary = {}
for name in ("native-on", "native-probe", "native-noon"):
    lines = (root / name / "sunlight.csv").read_text().splitlines()
    assert lines[-1] == "# complete", name
    rows = list(csv.DictReader(line for line in lines if not line.startswith("#")))
    assert len(rows) == 300, (name, len(rows))
    positive = 0
    for row in rows:
        alpha, visibility, strength = (float(row[key]) for key in
                                       ("sun_alpha", "visibility", "prepared_strength"))
        assert all(math.isfinite(v) for v in (alpha, visibility, strength))
        assert abs(strength - 0.1 * alpha * visibility) < 0.000002
        assert row["outdoors"] == "1"
        assert all(row[key] == "0" for key in ("underwater", "pause", "event", "twilight", "hidden"))
        assert abs(float(row["sun_u"]) - 0.5) < 0.000002
        assert abs(float(row["sun_v"]) - 0.5) < 0.000002
        positive += strength > 0
    assert json.loads((root / name / "exit.json").read_text())["exit_code"] == 0
    summary[name] = {"frames": len(rows), "positive_source_frames": positive,
                     "last_visibility": float(rows[-1]["visibility"]),
                     "last_strength": float(rows[-1]["prepared_strength"])}
    if name != "native-noon":
        assert positive == 0
    else:
        assert positive > 200 and float(rows[-1]["visibility"]) == 1
    if "peek_z0" in rows[-1]:
        last = rows[-1]
        assert last["weather"] == "0" and last["snow"] == "0"
        summary[name]["last_peek_z"] = [int(last[f"peek_z{i}"]) for i in range(5)]
        if name == "native-probe":
            assert int(last["peek_z0"]) < 0xFFFFFF
            assert any(int(last[f"peek_z{i}"]) == 0xFFFFFF for i in range(1, 5))
        else:
            assert all(int(last[f"peek_z{i}"]) == 0xFFFFFF for i in range(5))

log = (root / "native-noon/runtime.log").read_text()
assert "Sunlight shafts active:" in log
assert "Contact shading shutdown: compute=0 draw=2120 target_sets=0" in log
assert "all mods unloaded" in log
assert "reloading from" in log
gpu = (root / "gpu-tests.log").read_text()
assert "44 ambient pixel cases: PASS" in gpu and ": FAIL" not in gpu
assert "distant_lifted=2300 bound_failures=0" in gpu
print(json.dumps(summary, indent=2))
