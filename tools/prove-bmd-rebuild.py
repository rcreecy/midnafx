"""Rebuild a sampled BMD in memory with per-corner normal indices.

This is an offline representation/offset proof. It does not write game assets or
exercise the game's model loader, PC display-list optimizer, or CPU skinning.
"""

import argparse
import hashlib
import importlib.util
import json
import struct
from pathlib import Path


def sibling(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), Path(__file__).with_name(name))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


proof = sibling("prove-normal-reindex.py")
bmd = proof.bmd
topology = proof.topology


def align(value, boundary=32):
    return (value + boundary - 1) // boundary * boundary


def put32(data, offset, value):
    struct.pack_into(">I", data, offset, value)


def rebuild(data):
    evidence, normals, groups = proof.prove(data, return_artifacts=True)
    parts = proof.blocks(data)
    vtx, vtx_size = parts[b"VTX1"]
    shp, shp_size = parts[b"SHP1"]

    old_vtx = data[vtx:vtx + vtx_size]
    normal_at = bmd.be32(old_vtx, 16)
    following = [bmd.be32(old_vtx, offset) for offset in range(20, 64, 4)]
    normal_end = min((offset for offset in following if offset > normal_at), default=vtx_size)
    if normal_end == vtx_size:
        raise ValueError("normal array must have a following array for this proof")
    expanded = b"".join(normals)
    new_normal_end = align(normal_at + len(expanded))
    shift_vtx = new_normal_end - normal_end
    if shift_vtx < 0:
        raise ValueError("unexpected shrinking normal array")
    new_vtx = bytearray(old_vtx[:normal_at] + expanded +
                        bytes(new_normal_end - normal_at - len(expanded)) + old_vtx[normal_end:])
    for offset in range(20, 64, 4):
        value = bmd.be32(new_vtx, offset)
        if value >= normal_end:
            put32(new_vtx, offset, value + shift_vtx)
    put32(new_vtx, 4, len(new_vtx))

    old_shp = data[shp:shp + shp_size]
    dl_at = bmd.be32(old_shp, 32)
    dl_end = bmd.be32(old_shp, 36)
    draw_table = bmd.be32(old_shp, 40)
    if not (0 < dl_at < dl_end <= draw_table < shp_size):
        raise ValueError("unsupported SHP1 section order")
    shape_count = bmd.be16(old_shp, 8)
    init_at = bmd.be32(old_shp, 12)
    index_at = bmd.be32(old_shp, 16)
    used_draws = {}
    display_lists = bytearray()
    for shape, group, draws in groups:
        init = init_at + bmd.be16(old_shp, index_at + shape * 2) * 40
        draw_index = bmd.be16(old_shp, init + 8) + group
        if draw_index in used_draws:
            raise ValueError("shared draw-table entry")
        start = len(display_lists)
        payload = b"".join(draws)
        display_lists.extend(payload)
        display_lists.extend(bytes(align(len(display_lists)) - len(display_lists)))
        used_draws[draw_index] = (len(display_lists) - start, start)
    if len(groups) != evidence["matrixGroups"] or len(used_draws) != len(groups):
        raise ValueError("draw group mismatch")
    shift_shp = len(display_lists) - (dl_end - dl_at)
    new_shp = bytearray(old_shp[:dl_at] + display_lists + old_shp[dl_end:])
    for field in (36, 40):
        put32(new_shp, field, bmd.be32(old_shp, field) + shift_shp)
    new_draw_table = draw_table + shift_shp
    for index, (size, offset) in used_draws.items():
        entry = new_draw_table + index * 8
        if entry + 8 > len(new_shp):
            raise ValueError("draw-table entry out of bounds")
        put32(new_shp, entry, size)
        put32(new_shp, entry + 4, offset)
    put32(new_shp, 4, len(new_shp))

    rebuilt = bytearray(data[:32])
    cursor = 32
    for _ in range(bmd.be32(data, 12)):
        size = bmd.be32(data, cursor + 4)
        tag = data[cursor:cursor + 4]
        rebuilt.extend(new_vtx if tag == b"VTX1" else new_shp if tag == b"SHP1"
                       else data[cursor:cursor + size])
        cursor += size
    if cursor != len(data):
        raise ValueError("trailing bytes after BMD blocks")
    put32(rebuilt, 8, len(rebuilt))
    rebuilt = bytes(rebuilt)

    new_parts = proof.blocks(rebuilt)
    if len(rebuilt) != bmd.be32(rebuilt, 8):
        raise ValueError("BMD file size mismatch")
    for tag, (offset, size) in parts.items():
        if tag not in (b"VTX1", b"SHP1"):
            new_offset, new_size = new_parts[tag]
            if data[offset:offset + size] != rebuilt[new_offset:new_offset + new_size]:
                raise ValueError(f"unrelated block changed: {tag!r}")
    decoded = topology.decode(rebuilt)
    fmts = topology.formats(data, vtx, vtx_size)
    desc_at = bmd.be32(old_shp, 24)
    expected_hash = 14695981039346656037
    for shape, group, draws in groups:
        init = init_at + bmd.be16(old_shp, index_at + shape * 2) * 40
        offsets, stride = topology.descriptor(old_shp, desc_at + bmd.be16(old_shp, init + 4), fmts)
        pos_at, pos_kind = offsets[9]
        nrm_at, _ = offsets[10]
        for draw in draws:
            for value in (shape, group):
                expected_hash = proof.hash_u16(value, expected_hash)
            for corner in range(3):
                record = draw[3 + corner * stride:3 + (corner + 1) * stride]
                position = record[pos_at] if pos_kind == 2 else bmd.be16(record, pos_at)
                expected_hash = proof.hash_u16(position, expected_hash)
                expected_hash = proof.hash_u16(bmd.be16(record, nrm_at), expected_hash)
    if (decoded["triangles"] != evidence["triangles"] or
            decoded["shapes"] != shape_count or
            decoded["normals"] < evidence["rewrittenNormalCount"] or
            decoded["cornerHash"] != f"{expected_hash:016x}"):
        raise ValueError("independent rebuilt topology mismatch")
    return {**evidence, "rebuiltBytes": len(rebuilt), "originalBytes": len(data),
            "rebuiltNormalArrayCapacity": decoded["normals"],
            "rebuiltCornerHash": decoded["cornerHash"],
            "rebuiltSha256": hashlib.sha256(rebuilt).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    matches = [(tag, data) for tag, name, data in
               bmd.archive_entries(args.archive.read_bytes()) if name == args.name]
    if len(matches) != 1:
        raise ValueError(f"expected one {args.name!r} resource, found {len(matches)}")
    tag, data = matches[0]
    print(json.dumps({"archive": args.archive.name, "file": args.name, "tag": tag,
                      **rebuild(data)}, separators=(",", ":")))


if __name__ == "__main__":
    main()
