"""Read-only summary of J3D BMD resources in a GameCube RARC archive.

Accepts game-owned .arc files extracted from a disc; never writes asset data.
"""

import argparse
import struct
from pathlib import Path


def be16(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def be32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def yaz0(data):
    if data[:4] != b"Yaz0":
        return data
    if len(data) < 16:
        raise ValueError("truncated Yaz0 header")
    size = be32(data, 4)
    if size > 512 * 1024 * 1024:
        raise ValueError("Yaz0 output exceeds 512 MiB")
    out = bytearray(size)
    src = 16
    dst = 0
    bits = 0
    code = 0
    while dst < size:
        if bits == 0:
            if src >= len(data):
                raise ValueError("truncated Yaz0 control byte")
            code = data[src]
            src += 1
            bits = 8
        if code & 0x80:
            if src >= len(data):
                raise ValueError("truncated Yaz0 literal")
            out[dst] = data[src]
            src += 1
            dst += 1
        else:
            if src + 2 > len(data):
                raise ValueError("truncated Yaz0 back-reference")
            a, b = data[src:src + 2]
            src += 2
            distance = ((a & 15) << 8) | b
            count = a >> 4
            if count == 0:
                if src >= len(data):
                    raise ValueError("truncated Yaz0 length")
                count = data[src] + 18
                src += 1
            else:
                count += 2
            source = dst - distance - 1
            if source < 0 or dst + count > size:
                raise ValueError("invalid Yaz0 back-reference")
            for _ in range(count):
                out[dst] = out[source]
                dst += 1
                source += 1
        code <<= 1
        bits -= 1
    return bytes(out)


def archive_entries(data):
    data = yaz0(data)
    if len(data) < 64 or data[:4] != b"RARC":
        raise ValueError("expected RARC or Yaz0-compressed RARC")
    declared_size = be32(data, 4)
    if declared_size < 64 or declared_size > len(data):
        raise ValueError("invalid RARC length")
    data = data[:declared_size]
    info = be32(data, 8)
    if info + 32 > len(data):
        raise ValueError("invalid RARC information offset")
    node_base = info + be32(data, info + 4)
    file_base = info + be32(data, info + 12)
    string_base = info + be32(data, info + 20)
    resource_base = info + be32(data, 12)
    file_count = be32(data, info + 8)
    node_count = be32(data, info)
    string_size = be32(data, info + 16)
    if (node_base + node_count * 16 > len(data) or
            file_base + file_count * 20 > len(data) or
            string_base + string_size > len(data) or resource_base > len(data)):
        raise ValueError("RARC table exceeds archive length")
    for node in range(node_count):
        n = node_base + node * 16
        tag = data[n:n + 4].decode("ascii", "replace")
        count, first = be16(data, n + 10), be32(data, n + 12)
        for index in range(first, min(first + count, file_count)):
            entry = file_base + index * 20
            flags_and_name = be32(data, entry + 4)
            if flags_and_name >> 24 & 2:
                continue
            name_at = string_base + (flags_and_name & 0xFFFFFF)
            if name_at >= string_base + string_size:
                raise ValueError("RARC name offset exceeds string table")
            name_end = data.index(0, name_at, string_base + string_size)
            name = data[name_at:name_end].decode("ascii", "replace")
            start = resource_base + be32(data, entry + 8)
            length = be32(data, entry + 12)
            if start + length > len(data):
                raise ValueError("RARC payload exceeds archive length")
            yield tag, name, data[start:start + length]


def model_summary(data):
    if len(data) < 32:
        raise ValueError("truncated J3D header")
    if data[:4] not in (b"J3D2", b"J3D1") or data[4:8] not in (b"bmd2", b"bmd3"):
        return None
    blocks = {}
    offset = 32
    for _ in range(be32(data, 12)):
        if offset + 8 > len(data):
            raise ValueError("truncated J3D block")
        tag = data[offset:offset + 4]
        size = be32(data, offset + 4)
        if size < 8 or offset + size > len(data):
            raise ValueError("invalid J3D block size")
        blocks[tag] = (offset, size)
        offset += size
    if b"VTX1" not in blocks:
        return None
    vertex, vertex_size = blocks[b"VTX1"]
    if vertex_size < 64:
        raise ValueError("truncated VTX1 header")
    fmt_ptr = be32(data, vertex + 8)
    normal_ptr = be32(data, vertex + 16)
    nbt_ptr = be32(data, vertex + 20)
    pos_ptr = be32(data, vertex + 12)
    if normal_ptr > vertex_size or pos_ptr > vertex_size or fmt_ptr > vertex_size:
        raise ValueError("VTX1 offset outside block")
    formats = {}
    for i in range(32):
        at = vertex + fmt_ptr + i * 16
        if at + 16 > vertex + vertex_size:
            break
        attr, count, kind = struct.unpack_from(">III", data, at)
        if attr == 255:
            break
        formats[attr] = (count, kind, data[at + 12])
    envelopes = be16(data, blocks[b"EVP1"][0] + 8) if b"EVP1" in blocks else 0
    shapes = be16(data, blocks[b"SHP1"][0] + 8) if b"SHP1" in blocks else 0
    following = [be32(data, vertex + at) for at in range(20, 64, 4)]
    normal_end = min((offset for offset in following if offset > normal_ptr),
                     default=vertex_size)
    normal_format = formats.get(10)
    normal_stride = (12 if normal_format and normal_format[:2] == (0, 4) else
                     6 if normal_format and normal_format[0] == 0 else 0)
    return {
        "normal": normal_format,
        "position": formats.get(9),
        "normal_offset": normal_ptr,
        "normal_records": ((normal_end - normal_ptr) // normal_stride
                           if normal_ptr and normal_stride else 0),
        "position_offset": pos_ptr,
        "nbt": bool(nbt_ptr),
        "envelopes": envelopes,
        "shapes": shapes,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--find", help="Print archive members containing this ASCII text")
    args = parser.parse_args()
    paths = (file for item in args.archives for file in
             (item.rglob("*.arc") if item.is_dir() else [item]))
    for path in paths:
        try:
            for tag, name, payload in archive_entries(path.read_bytes()):
                if args.find is not None:
                    if args.find.encode("ascii") in payload:
                        print(f"{path}: {tag}/{name}")
                    continue
                summary = model_summary(payload)
                if summary is not None:
                    print(f"{path.name}/{name} tag={tag} bytes={len(payload)} {summary}")
        except (ValueError, IndexError, struct.error) as exc:
            print(f"{path}: {exc}")


if __name__ == "__main__":
    main()
