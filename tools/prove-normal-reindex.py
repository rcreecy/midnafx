"""Offline, read-only per-corner normal-index rewrite proof for one game BMD.

Expands raw GX triangles/strips/fans/quads into three-vertex strips, gives every
surviving triangle corner a fresh normal index whose bytes copy its original,
then independently parses the rewritten in-memory draws. No game asset is
written or packaged. This proves representation capacity, not safe runtime
ownership, skinning, or improved shading.
"""

import argparse
import hashlib
import importlib.util
import json
import math
import struct
from pathlib import Path


def load_sibling(name):
    path = Path(__file__).with_name(name)
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bmd = load_sibling("inspect-bmd.py")
topology = load_sibling("inspect-topology.py")


def blocks(data):
    if len(data) < 32 or data[:4] not in (b"J3D1", b"J3D2"):
        raise ValueError("invalid J3D header")
    result = {}
    at = 32
    for _ in range(bmd.be32(data, 12)):
        if at + 8 > len(data):
            raise ValueError("truncated J3D block")
        size = bmd.be32(data, at + 4)
        if size < 8 or at + size > len(data):
            raise ValueError("invalid J3D block length")
        result[data[at:at + 4]] = (at, size)
        at += size
    return result


def normal_array(data, vtx, vtx_size, fmts):
    offset = bmd.be32(data, vtx + 16)
    later = [bmd.be32(data, vtx + i) for i in range(20, 64, 4)]
    end = min((n for n in later if n > offset), default=vtx_size)
    if offset >= end or end > vtx_size or fmts[10][0] != 0:
        raise ValueError("unsupported normal array")
    stride = {3: 6, 4: 12}.get(fmts[10][1])
    if stride is None or end - offset < stride:
        raise ValueError("unsupported normal encoding or length")
    return [data[vtx + offset + i:vtx + offset + i + stride]
            for i in range(0, (end - offset) // stride * stride, stride)]


def nondegenerate(triangle, positions, pos_offset, pos_kind):
    indices = [record[pos_offset] if pos_kind == 2 else bmd.be16(record, pos_offset)
               for record in triangle]
    if any(index >= len(positions) for index in indices):
        raise ValueError("position index out of bounds")
    if len(set(indices)) < 3:
        return False
    p, q, r = (positions[index] for index in indices)
    a = tuple(q[i] - p[i] for i in range(3))
    b = tuple(r[i] - p[i] for i in range(3))
    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
             a[0] * b[1] - a[1] * b[0])
    length = math.sqrt(sum(value * value for value in cross))
    return math.isfinite(length) and length > 1e-8


def hash_u16(value, current):
    for shift in (0, 8):
        current = ((current ^ ((value >> shift) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return current


def prove(data):
    parts = blocks(data)
    vtx, vtx_size = parts[b"VTX1"]
    shp, shp_size = parts[b"SHP1"]
    fmts = topology.formats(data, vtx, vtx_size)
    positions = topology.positions(data, vtx, vtx_size, fmts)
    originals = normal_array(data, vtx, vtx_size, fmts)
    normal_values = list(originals)
    shape_count = bmd.be16(data, shp + 8)
    init_base = shp + bmd.be32(data, shp + 12)
    index_base = shp + bmd.be32(data, shp + 16)
    desc_base = shp + bmd.be32(data, shp + 24)
    dl_base = shp + bmd.be32(data, shp + 32)
    draw_base = shp + bmd.be32(data, shp + 40)
    expected = []
    rewritten = []
    raw_bytes = 0
    source_draws = 0
    for shape in range(shape_count):
        shape_init = init_base + bmd.be16(data, index_base + shape * 2) * 40
        group_count = bmd.be16(data, shape_init + 2)
        offsets, stride = topology.descriptor(
            data, desc_base + bmd.be16(data, shape_init + 4), fmts)
        pos_offset, pos_kind = offsets[9]
        nrm_offset, nrm_kind = offsets[10]
        if nrm_kind != 3:
            raise ValueError("proof requires GX_INDEX16 normals")
        draw_index = bmd.be16(data, shape_init + 8)
        for group in range(group_count):
            draw = draw_base + (draw_index + group) * 8
            at = dl_base + bmd.be32(data, draw + 4)
            end = at + bmd.be32(data, draw)
            if end > shp + shp_size:
                raise ValueError("draw exceeds SHP1")
            raw_bytes += end - at
            group_draws = []
            while at < end:
                cmd = data[at]
                if cmd == 0:
                    at += 1
                    continue
                primitive = cmd & 0xf8
                if primitive not in (0x80, 0x90, 0x98, 0xa0):
                    raise ValueError(f"unsupported GX command {cmd:#x}")
                count = bmd.be16(data, at + 1)
                source_draws += 1
                at += 3
                if at + count * stride > end:
                    raise ValueError("draw vertex overrun")
                vertices = [data[at + i * stride:at + (i + 1) * stride]
                            for i in range(count)]
                at += count * stride
                for triangle in topology.triangles_for_draw(primitive, vertices):
                    if not nondegenerate(triangle, positions, pos_offset, pos_kind):
                        continue
                    output = bytearray()
                    for record in triangle:
                        old_index = bmd.be16(record, nrm_offset)
                        if old_index >= len(originals) or len(normal_values) >= 65536:
                            raise ValueError("normal index out of bounds or 16-bit overflow")
                        new_index = len(normal_values)
                        normal_values.append(originals[old_index])
                        updated = bytearray(record)
                        struct.pack_into(">H", updated, nrm_offset, new_index)
                        output.extend(updated)
                        position = (record[pos_offset] if pos_kind == 2 else
                                    bmd.be16(record, pos_offset))
                        expected.append((shape, group, record, old_index, new_index,
                                         nrm_offset, stride, position))
                    group_draws.append(b"\x98\x00\x03" + output)
            rewritten.append((shape, group, group_draws))

    cursor = 0
    output_bytes = 0
    digest_before = hashlib.sha256()
    digest_after = hashlib.sha256()
    remapped_hash = 14695981039346656037
    for shape, group, draws in rewritten:
        for draw in draws:
            count = bmd.be16(draw, 1)
            if draw[0] != 0x98 or count != 3:
                raise ValueError("rewritten draw is not a three-vertex strip")
            stride = expected[cursor][6]
            if len(draw) != 3 + count * stride:
                raise ValueError("rewritten draw length mismatch")
            output_bytes += len(draw)
            remapped_hash = hash_u16(shape, remapped_hash)
            remapped_hash = hash_u16(group, remapped_hash)
            for i in range(count):
                record = draw[3 + i * stride:3 + (i + 1) * stride]
                (exp_shape, exp_group, original, old_index, new_index,
                 nrm_offset, _, position) = expected[cursor]
                if (shape, group) != (exp_shape, exp_group):
                    raise ValueError("shape or matrix group changed")
                if bmd.be16(record, nrm_offset) != new_index:
                    raise ValueError("rewritten normal index mismatch")
                if (record[:nrm_offset] + record[nrm_offset + 2:] !=
                        original[:nrm_offset] + original[nrm_offset + 2:]):
                    raise ValueError("non-normal corner attributes changed")
                if normal_values[new_index] != originals[old_index]:
                    raise ValueError("duplicated normal value changed")
                identity = (struct.pack(">HH", shape, group) +
                            original[:nrm_offset] + original[nrm_offset + 2:])
                digest_before.update(identity)
                digest_after.update(struct.pack(">HH", shape, group) +
                                    record[:nrm_offset] + record[nrm_offset + 2:])
                remapped_hash = hash_u16(position, remapped_hash)
                remapped_hash = hash_u16(old_index, remapped_hash)
                cursor += 1
    if cursor != len(expected) or digest_before.digest() != digest_after.digest():
        raise ValueError("corner sequence changed")
    independent = topology.decode(data)
    if independent["triangles"] * 3 != cursor:
        raise ValueError("triangle count differs from independent decoder")
    if independent["cornerHash"] != f"{remapped_hash:016x}":
        raise ValueError("rewritten topology differs from independent decoder")
    return {
        "originalCornerHash": independent["cornerHash"],
        "remappedCornerHash": f"{remapped_hash:016x}",
        "shapes": shape_count,
        "matrixGroups": len(rewritten),
        "triangles": cursor // 3,
        "originalNormalCount": len(originals),
        "rewrittenNormalCount": len(normal_values),
        "maxNormalIndex": len(normal_values) - 1,
        "originalDrawCount": source_draws,
        "rewrittenDrawCount": sum(len(draws) for _, _, draws in rewritten),
        "originalDrawBytes": raw_bytes,
        "rewrittenDrawBytes": output_bytes,
        "nonNormalCornerSha256": digest_after.hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--name", required=True, help="Exact BMD resource name")
    args = parser.parse_args()
    matches = [(tag, data) for tag, name, data in
               bmd.archive_entries(args.archive.read_bytes()) if name == args.name]
    if len(matches) != 1:
        raise ValueError(f"expected one {args.name!r} resource, found {len(matches)}")
    tag, data = matches[0]
    print(json.dumps({"archive": args.archive.name, "file": args.name, "tag": tag,
                      **prove(data)}, separators=(",", ":")))


if __name__ == "__main__":
    main()
