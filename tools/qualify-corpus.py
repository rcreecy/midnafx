"""Run the production decoder/classifier on local original J3D archive resources.

No assets are distributed. Results are offline structural evidence, not runtime
skinning, restoration, visibility or art-quality evidence.
"""
import argparse
import importlib.util
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def sibling(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bmd = sibling("inspect-bmd.py")
topology = sibling("inspect-topology.py")
reindex = sibling("prove-normal-reindex.py")


def export(data):
    parts = reindex.blocks(data)
    vtx, vsize = parts[b"VTX1"]
    shp, ssize = parts[b"SHP1"]
    fmts = topology.formats(data, vtx, vsize)
    points = topology.positions(data, vtx, vsize, fmts)
    normal_bytes = reindex.normal_array(data, vtx, vsize, fmts)
    kind, fraction = fmts[10][1:]
    if fraction > 15:
        raise ValueError("unsupported normal fraction")
    normals = [struct.unpack(">fff" if kind == 4 else ">hhh", value) for value in normal_bytes]
    if kind == 3:
        normals = [tuple(c / (1 << fraction) for c in value) for value in normals]
    shape_count = bmd.be16(data, shp + 8)
    # Treat shape boundaries as material boundaries. This intentionally
    # over-preserves splits when two shapes share one material; offline SAFE
    # does not imply runtime SAFE with the actual material association.
    init = shp + bmd.be32(data, shp + 12)
    remap = shp + bmd.be32(data, shp + 16)
    descriptors = shp + bmd.be32(data, shp + 24)
    lists = shp + bmd.be32(data, shp + 32)
    draws = shp + bmd.be32(data, shp + 40)
    count = lambda block: bmd.be16(data, parts[block][0] + 8)
    out = bytearray(b"MFXQ0001")
    out += struct.pack("<7I", len(points), len(normals), shape_count, len(fmts),
                       count(b"EVP1"), count(b"JNT1"), count(b"DRW1"))
    for value in points + normals:
        out += struct.pack("<3f", *value)
    for attr, (components, encoding, _) in fmts.items():
        out += struct.pack("<3B", attr, components, encoding)
    for shape in range(shape_count):
        record = init + bmd.be16(data, remap + shape * 2) * 40
        group_count = bmd.be16(data, record + 2)
        at = descriptors + bmd.be16(data, record + 4)
        attrs = []
        for _ in range(32):
            attr, mode = struct.unpack_from(">II", data, at)
            at += 8
            if attr == 255:
                break
            attrs.append((attr, mode))
        else:
            raise ValueError("unterminated descriptor")
        out += struct.pack("<3I", shape, len(attrs), group_count)
        for attr in attrs:
            out += struct.pack("<2B", *attr)
        draw_index = bmd.be16(data, record + 8)
        for group in range(group_count):
            draw = draws + (draw_index + group) * 8
            size = bmd.be32(data, draw)
            begin = lists + bmd.be32(data, draw + 4)
            if begin < shp or begin + size > shp + ssize:
                raise ValueError("display list bounds")
            out += struct.pack("<I", size) + data[begin:begin + size]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("archives", nargs="+", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="midnafx-corpus-") as directory:
        fixture = Path(directory) / "model.bin"
        for archive in args.archives:
            for _, name, data in bmd.archive_entries(archive.read_bytes()):
                if not name.endswith(".bmd"):
                    continue
                record = {"archive": str(archive), "file": name, "evidence": "offline-original"}
                try:
                    fixture.write_bytes(export(data))
                    result = subprocess.run([str(args.runner.resolve()), str(fixture)],
                                            capture_output=True, text=True, check=True)
                    record.update(json.loads(result.stdout))
                except (ValueError, KeyError, IndexError, struct.error, subprocess.CalledProcessError) as error:
                    record.update(classification="UNSUPPORTED", reason=str(error))
                print(json.dumps(record), flush=True)


if __name__ == "__main__":
    main()
