"""Inventory water-like J3D material names in extracted TP RARC archives.

This read-only tool emits JSON lines. It treats names as candidates only; runtime
actor/material identity remains the authority for the product allowlist.
"""

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


def material_names(data):
    if len(data) < 32 or data[:4] not in (b"J3D1", b"J3D2"):
        return []
    offset = 32
    for _ in range(be32(data, 12)):
        if offset + 8 > len(data):
            raise ValueError("truncated J3D block")
        size = be32(data, offset + 4)
        if size < 8 or offset + size > len(data):
            raise ValueError("invalid J3D block")
        if data[offset:offset + 4] in (b"MAT2", b"MAT3"):
            count = be16(data, offset + 8)
            table = offset + be32(data, offset + 0x14)
            if table + 4 + count * 4 > offset + size:
                raise ValueError("invalid material name table")
            names = []
            for index in range(count):
                name_at = table + be16(data, table + 4 + index * 4 + 2)
                if name_at < table or name_at >= offset + size:
                    raise ValueError("invalid material name offset")
                end = data.index(0, name_at, offset + size)
                names.append(data[name_at:end].decode("ascii", "replace"))
            return names
        offset += size
    return []


def water_like(name):
    lower = name.lower()
    words = ("water", "mizu", "sui", "nigori", "mera", "onsen", "pool", "river")
    controls = ("ma02", "ma03", "ma06", "ma09", "ma17", "ma19")
    return any(word in lower for word in words) or any(code in lower for code in controls)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", type=Path)
    parser.add_argument("--all", action="store_true", help="emit every material name")
    args = parser.parse_args()
    inspector = load_inspector()
    archives = (path for root in args.roots for path in
                (root.rglob("*.arc") if root.is_dir() else [root]))
    for archive in archives:
        try:
            for tag, model, payload in inspector.archive_entries(archive.read_bytes()):
                for index, name in enumerate(material_names(payload)):
                    if args.all or water_like(name):
                        print(json.dumps({"archive": str(archive), "tag": tag,
                                          "model": model, "material": index,
                                          "name": name}, separators=(",", ":")))
        except (OSError, ValueError, IndexError, struct.error) as exc:
            print(json.dumps({"archive": str(archive), "error": str(exc)},
                             separators=(",", ":")))


if __name__ == "__main__":
    main()
