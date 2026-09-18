"""Read-only, independent GX topology audit of original game-owned BMDs.

Unlike MidnaFX's runtime decoder this reads SHP1's original display lists from
RARC. It never writes or packages game data. Output is one JSON object per BMD.
"""

import argparse
import importlib.util
import json
import math
import struct
from pathlib import Path

spec = importlib.util.spec_from_file_location("inspect_bmd", Path(__file__).with_name("inspect-bmd.py"))
bmd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bmd)


def formats(data, vtx, size):
    at = vtx + bmd.be32(data, vtx + 8)
    result = {}
    while at + 16 <= vtx + size:
        attr, count, kind = struct.unpack_from(">III", data, at)
        if attr == 255:
            return result
        result[attr] = (count, kind, data[at + 12])
        at += 16
    raise ValueError("unterminated VTX1 format list")


def direct_size(attr, fmt):
    if attr <= 8:
        return 1
    if fmt is None:
        raise ValueError("missing direct attribute format")
    count, kind, _ = fmt
    if attr in (11, 12):
        return (2, 3, 4, 2, 3, 4)[kind]
    component = (1, 1, 2, 2, 4)[kind]
    if attr == 9:
        n = (2, 3)[count]
    elif attr == 10:
        n = (3, 9, 9)[count]
    elif 13 <= attr <= 20:
        n = (1, 2)[count]
    else:
        raise ValueError("unsupported direct attribute")
    return component * n


def descriptor(data, begin, fmts):
    at = begin
    offsets = {}
    stride = 0
    for _ in range(32):
        attr, kind = struct.unpack_from(">II", data, at)
        at += 8
        if attr == 255:
            if 9 not in offsets or 10 not in offsets:
                raise ValueError("missing indexed position/normal")
            return offsets, stride
        if attr > 20 or kind not in (1, 2, 3) or attr in offsets:
            raise ValueError("invalid descriptor")
        size = direct_size(attr, fmts.get(attr)) if kind == 1 else kind - 1
        if attr == 10 and kind in (2, 3) and fmts.get(10, (0,))[0] == 2:
            size *= 3
        offsets[attr] = (stride, kind)
        stride += size
    raise ValueError("unterminated descriptor")


def positions(data, vtx, size, fmts):
    pos_offset = bmd.be32(data, vtx + 12)
    next_offsets = [bmd.be32(data, vtx + i) for i in range(16, 64, 4)]
    end = min((n for n in next_offsets if n > pos_offset), default=size)
    count, kind, frac = fmts[9]
    if count != 1 or kind not in (3, 4):
        raise ValueError("unsupported position format")
    stride = 6 if kind == 3 else 12
    n = (end - pos_offset) // stride
    result = []
    for i in range(n):
        at = vtx + pos_offset + i * stride
        xyz = struct.unpack_from(">hhh" if kind == 3 else ">fff", data, at)
        result.append(tuple(v / (1 << frac) if kind == 3 else v for v in xyz))
    return result


def triangles_for_draw(primitive, vertices):
    n = len(vertices)
    if primitive == 0x90:
        if n % 3:
            raise ValueError("triangle count")
        for i in range(0, n, 3):
            yield vertices[i], vertices[i + 1], vertices[i + 2]
    elif primitive == 0x98:
        for i in range(2, n):
            yield (vertices[i - 2], vertices[i - 1], vertices[i]) if not i & 1 else (
                vertices[i - 1], vertices[i - 2], vertices[i])
    elif primitive == 0xa0:
        for i in range(2, n):
            yield vertices[0], vertices[i - 1], vertices[i]
    elif primitive == 0x80:
        if n % 4:
            raise ValueError("quad count")
        for i in range(0, n, 4):
            yield vertices[i], vertices[i + 1], vertices[i + 2]
            yield vertices[i + 2], vertices[i + 3], vertices[i]
    else:
        raise ValueError(f"unsupported triangle primitive {primitive:#x}")


def decode(data, normal_usage=False):
    blocks = {}
    at = 32
    for _ in range(bmd.be32(data, 12)):
        size = bmd.be32(data, at + 4)
        if size < 8 or at + size > len(data):
            raise ValueError("invalid J3D block")
        blocks[data[at:at + 4]] = (at, size)
        at += size
    vtx, vtx_size = blocks[b"VTX1"]
    shp, shp_size = blocks[b"SHP1"]
    fmts = formats(data, vtx, vtx_size)
    pos = positions(data, vtx, vtx_size, fmts)
    norm_offset = bmd.be32(data, vtx + 16)
    later = [bmd.be32(data, vtx + i) for i in range(20, 64, 4)]
    norm_end = min((n for n in later if n > norm_offset), default=vtx_size)
    norm_stride = 12 if fmts[10][1] == 4 else 6
    normal_count = (norm_end - norm_offset) // norm_stride
    shape_count = bmd.be16(data, shp + 8)
    init_base = shp + bmd.be32(data, shp + 12)
    index_base = shp + bmd.be32(data, shp + 16)
    desc_base = shp + bmd.be32(data, shp + 24)
    dl_base = shp + bmd.be32(data, shp + 32)
    draw_base = shp + bmd.be32(data, shp + 40)
    tris = []
    primitive_counts = {}
    normal_index_widths = set()
    degenerate = 0
    for shape in range(shape_count):
        shape_init = init_base + bmd.be16(data, index_base + shape * 2) * 40
        group_count = bmd.be16(data, shape_init + 2)
        desc_at = desc_base + bmd.be16(data, shape_init + 4)
        offsets, stride = descriptor(data, desc_at, fmts)
        normal_index_widths.add(8 if offsets[10][1] == 2 else 16)
        draw_index = bmd.be16(data, shape_init + 8)
        for group in range(group_count):
            draw = draw_base + (draw_index + group) * 8
            p = dl_base + bmd.be32(data, draw + 4)
            end = p + bmd.be32(data, draw)
            if end > shp + shp_size:
                raise ValueError("display list exceeds SHP1")
            while p < end:
                cmd = data[p]
                if cmd == 0:
                    p += 1
                    continue
                if cmd < 0x80:
                    raise ValueError(f"unexpected raw GX opcode {cmd:#x}")
                primitive = cmd & 0xf8
                count = bmd.be16(data, p + 1)
                p += 3
                if p + count * stride > end:
                    raise ValueError("raw draw overrun")
                corners = []
                for i in range(count):
                    base = p + i * stride
                    indices = []
                    for attr in (9, 10):
                        offset, kind = offsets[attr]
                        if kind == 1:
                            raise ValueError("direct position/normal")
                        idx = data[base + offset] if kind == 2 else bmd.be16(data, base + offset)
                        indices.append(idx)
                    if indices[0] >= len(pos) or indices[1] >= normal_count:
                        raise ValueError("index out of bounds")
                    corners.append(tuple(indices))
                p += count * stride
                primitive_counts[hex(primitive)] = primitive_counts.get(hex(primitive), 0) + 1
                for tri in triangles_for_draw(primitive, corners):
                    p0, p1, p2 = (pos[c[0]] for c in tri)
                    a = tuple(p1[i] - p0[i] for i in range(3))
                    b = tuple(p2[i] - p0[i] for i in range(3))
                    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                             a[0] * b[1] - a[1] * b[0])
                    magnitude = math.sqrt(sum(x * x for x in cross))
                    if len({c[0] for c in tri}) < 3 or not math.isfinite(magnitude) or magnitude <= 1e-8:
                        degenerate += 1
                        continue
                    tris.append((shape, group, tri))
    hash_value = 14695981039346656037
    for shape, group, tri in tris:
        for value in (shape, group, *(x for corner in tri for x in corner)):
            for shift in (0, 8):
                hash_value = (hash_value ^ ((value >> shift) & 255)) * 1099511628211 & ((1 << 64) - 1)
    pns = {}
    for _, _, tri in tris:
        for p, n in tri:
            pns.setdefault(p, set()).add(n)
    result = {"positions": len(pos), "normals": normal_count, "shapes": shape_count,
            "primitives": primitive_counts, "triangles": len(tris), "degenerate": degenerate,
            "uniquePositions": len(pns), "uniqueNormals": len({n for ns in pns.values() for n in ns}),
            "positionNormalSplits": sum(len(ns) > 1 for ns in pns.values()),
            "cornerHash": f"{hash_value:016x}"}
    if normal_usage:
        usage = {}
        for shape, group, tri in tris:
            for position, normal in tri:
                entry = usage.setdefault(normal, {"positions": set(), "shapes": set(),
                                                  "matrixGroups": set(), "references": 0})
                entry["positions"].add(position)
                entry["shapes"].add(shape)
                entry["matrixGroups"].add((shape, group))
                entry["references"] += 1
        result["normalUsage"] = {
            "indexWidths": sorted(normal_index_widths),
            "reusedAcrossPositions": sum(len(item["positions"]) > 1 for item in usage.values()),
            "reusedAcrossShapes": sum(len(item["shapes"]) > 1 for item in usage.values()),
            "reusedAcrossMatrixGroups": sum(len(item["matrixGroups"]) > 1 for item in usage.values()),
            "maxPositionsPerNormal": max((len(item["positions"]) for item in usage.values()), default=0),
            "maxMatrixGroupsPerNormal": max((len(item["matrixGroups"]) for item in usage.values()), default=0),
            "maxReferencesPerNormal": max((item["references"] for item in usage.values()), default=0),
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--name", action="append", help="Select BMD file name; repeatable")
    parser.add_argument("--normal-usage", action="store_true",
                        help="Report normal-index reuse across positions and matrix groups")
    args = parser.parse_args()
    for path in args.archives:
        for tag, name, data in bmd.archive_entries(path.read_bytes()):
            if args.name and name not in args.name:
                continue
            if not name.endswith(".bmd"):
                continue
            try:
                result = decode(data, args.normal_usage)
                print(json.dumps({"archive": path.name, "file": name, "tag": tag, **result},
                                 separators=(",", ":")))
            except (ValueError, KeyError, IndexError, struct.error) as exc:
                print(json.dumps({"archive": path.name, "file": name, "error": str(exc)}))


if __name__ == "__main__":
    main()
