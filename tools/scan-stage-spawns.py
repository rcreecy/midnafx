"""List TP player start points from extracted stage room archives as JSON lines."""

import argparse
import importlib.util
import json
import struct
from pathlib import Path


def load_inspector():
    path = Path(__file__).with_name("inspect-bmd.py")
    spec = importlib.util.spec_from_file_location("midnafx_inspect_bmd", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def be16(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def be32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def befloat(data, offset):
    return struct.unpack_from(">f", data, offset)[0]


def spawns(data):
    if len(data) < 4:
        return []
    chunks = be32(data, 0)
    if chunks > 256 or 4 + chunks * 12 > len(data):
        raise ValueError("invalid stage chunk table")
    result = []
    for index in range(chunks):
        at = 4 + index * 12
        if data[at:at + 4] != b"PLYR":
            continue
        count, start = be32(data, at + 4), be32(data, at + 8)
        if start + count * 0x20 > len(data):
            raise ValueError("invalid PLYR chunk")
        for entry in range(count):
            item = start + entry * 0x20
            result.append({
                "point": be16(data, item + 0x1C),
                "x": befloat(data, item + 0x0C),
                "y": befloat(data, item + 0x10),
                "z": befloat(data, item + 0x14),
            })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage_root", type=Path)
    args = parser.parse_args()
    inspector = load_inspector()
    for archive in sorted(args.stage_root.rglob("R??_??.arc")):
        stage = archive.parent.name
        room = int(archive.stem[1:3])
        layer = int(archive.stem[4:6])
        try:
            for _tag, name, payload in inspector.archive_entries(archive.read_bytes()):
                if name not in ("room.dzr", "stage.dzs"):
                    continue
                for spawn in spawns(payload):
                    print(json.dumps({"stage": stage, "room": room, "layer": layer,
                                      **spawn}, separators=(",", ":")))
        except (OSError, ValueError, IndexError, struct.error) as exc:
            print(json.dumps({"archive": str(archive), "error": str(exc)},
                             separators=(",", ":")))


if __name__ == "__main__":
    main()
