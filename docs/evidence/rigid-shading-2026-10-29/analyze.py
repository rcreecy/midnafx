"""Check retained native observations without requiring game assets."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
summary = {}
for name, expected_arrays, expected_restores in (
    ("resident-off", 3, 3),
    ("partial-resident", 20, 20),
    ("final-resident", 20, 40),
):
    folder = root / name
    log = (folder / "native.log").read_text(encoding="utf-8")
    session = json.loads((folder / "session.json").read_text(encoding="utf-8-sig"))
    assert json.loads((folder / "exit.json").read_text(encoding="utf-8-sig"))["exit_code"] == 0
    applied = re.findall(r"Geometry qualification: (\S+) class=SAFE knownGood=no .*applied=yes", log)
    restored = re.findall(r"Geometry restore proof: bytes=(\d+) hash=([0-9a-f]+) exact=(\S+)", log)
    sweeps = [tuple(map(int, match)) for match in re.findall(
        r"resident sweep complete: examined=(\d+) applied=(\d+) skipped=(\d+)", log)]
    assert len(set(applied)) == expected_arrays, (name, applied)
    assert len(restored) == expected_restores, (name, restored)
    assert all(exact == "yes" for _, _, exact in restored)
    assert "Geometry restore ownership:" not in log
    assert "all mods unloaded" in log
    assert sweeps and sweeps[0][1] == expected_arrays
    if name == "final-resident":
        assert [sweep[1] for sweep in sweeps] == [20, 20, 0], sweeps
        assert len(applied) == 40
        assert log.count("Rigid shading guards ready:") == 2
        assert "reloading from" in log
    summary[name] = {
        "package_sha256": session["package_sha256"],
        "host_sha256": session["host_sha256"],
        "sweeps": [dict(zip(("examined", "applied", "skipped"), row)) for row in sweeps],
        "distinct_loaded_arrays": sorted(set(applied)),
        "application_events": len(applied),
        "exact_restoration_events": len(restored),
        "restored_bytes": sum(int(size) for size, _, _ in restored),
        "exit_code": 0,
    }

(root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print("Native coverage, reactivation, restoration, reload exclusion and exit assertions pass.")
