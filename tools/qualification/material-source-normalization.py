"""Verify the pinned native loader's transformation of the original pumpkin BMD.

Inputs stay local. This tool writes metadata only, never model or texture bytes.
It models the little-endian Dusklight loader, not a general BMD conversion API.
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path


SOURCE_SIZE = 17824
SOURCE_FNV = "9b2ddb5ecbd95421"
NORMALIZED_FNV = "7073c7ae329d074d"
PADDING = tuple(offset for first in (0xCD, 0xDD, 0xED, 0xFD)
                for offset in range(first, first + 3))


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"


def read_archive(path):
    parser_path = Path(__file__).resolve().parents[1] / "inspect-bmd.py"
    spec = importlib.util.spec_from_file_location("material_source_archive", parser_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    matches = [data for _, name, data in module.archive_entries(path.read_bytes())
               if name == "pumpkin.bmd"]
    if len(matches) != 1:
        raise ValueError("archive must contain exactly one pumpkin.bmd")
    return bytes(matches[0])


def simulate(source):
    if len(source) != SOURCE_SIZE or fnv(source) != SOURCE_FNV:
        raise ValueError("source must be the original 17,824-byte pumpkin BMD")
    if source[:8] != b"J3D2bmd3":
        raise ValueError("source has an unexpected BMD header")
    result = bytearray(source)
    swap_count = 0

    def integer(at, size):
        if at < 0 or at + size > len(source):
            raise ValueError("source metadata exceeds BMD bounds")
        return int.from_bytes(source[at:at + size], "big")

    def swap(at, size):
        nonlocal swap_count
        if at < 0 or at + size > len(result):
            raise ValueError("native conversion exceeds BMD bounds")
        result[at:at + size] = result[at:at + size][::-1]
        swap_count += 1

    blocks = {}
    at = 32
    for _ in range(integer(12, 4)):
        size = integer(at + 4, 4)
        tag = source[at:at + 4].decode("ascii")
        if size < 8 or at + size > len(source) or tag in blocks:
            raise ValueError("invalid BMD block layout")
        blocks[tag] = (at, size)
        at += size
    if at != len(source):
        raise ValueError("BMD block lengths do not cover the source")

    # J3DModelLoader::readVertex swaps format fields, then readVertexData swaps
    # every component through the next array offset, including native padding.
    vertex, size = blocks["VTX1"]
    at = vertex + integer(vertex + 8, 4)
    formats = {}
    padding = []
    while True:
        attribute, count, component_type = (integer(at + offset, 4) for offset in (0, 4, 8))
        formats[attribute] = (count, component_type)
        for offset in (0, 4, 8):
            swap(at + offset, 4)
        padding.extend(range(at + 13, at + 16))
        if attribute == 255:
            break
        at += 16
    if tuple(padding) != PADDING or any(source[offset] != 255 for offset in PADDING):
        raise ValueError("unexpected vertex format padding layout")
    offsets = [integer(vertex + 12 + 4 * index, 4) for index in range(13)]
    attributes = (9, 10, 25, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)
    for index, start in enumerate(offsets):
        if not start:
            continue
        end = next((offset for offset in offsets[index + 1:] if offset), size)
        _, component_type = formats[attributes[index]]
        if attributes[index] in (11, 12):
            stride = 2 if component_type in (0, 3) else 1
        else:
            stride = 4 if component_type == 4 else 2 if component_type in (2, 3) else 1
        if start > end or end > size or (end - start) % stride:
            raise ValueError("invalid vertex component bounds")
        if stride > 1:
            for at in range(vertex + start, vertex + end, stride):
                swap(at, stride)

    # J3DJointFactory swaps each remap index and the referenced joint's fields.
    # Padding and byte-sized fields remain unchanged.
    joint, _ = blocks["JNT1"]
    data = joint + integer(joint + 12, 4)
    indices = joint + integer(joint + 16, 4)
    for index in range(integer(joint + 8, 2)):
        remap = integer(indices + 2 * index, 2)
        swap(indices + 2 * index, 2)
        at = data + remap * 64
        swap(at, 2)
        for offset in (4, 8, 12, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60):
            swap(at + offset, 4)
        for offset in (16, 18, 20):
            swap(at + offset, 2)

    # J3DShapeFactory swaps complete descriptor records through the terminator
    # following the greatest descriptor-list starting offset.
    shape, _ = blocks["SHP1"]
    data = shape + integer(shape + 12, 4)
    indices = shape + integer(shape + 16, 4)
    descriptors = shape + integer(shape + 24, 4)
    last = descriptors + max(integer(data + integer(indices + 2 * index, 2) * 40 + 4, 2)
                             for index in range(integer(shape + 8, 2)))
    while integer(last, 4) != 255:
        last += 8
    for at in range(descriptors, last + 8, 4):
        swap(at, 4)

    # The aggregate returned by BE<GXVtxAttrFmtList>::swap has unspecified
    # padding. Use the original 0xff values without writing to the live source.
    for offset in PADDING:
        result[offset] = 255
    if fnv(result) != NORMALIZED_FNV:
        raise ValueError("simulated native source does not match the qualified normalized digest")
    return result, blocks, swap_count


def verify(source, post=None):
    expected, blocks, swap_count = simulate(source)
    report = {
        "schema": 1,
        "scope": "Pinned little-endian Dusklight loader; original pumpkin BMDR only",
        "source_bytes": len(source),
        "original_fnv1a64": fnv(source),
        "original_sha256": hashlib.sha256(source).hexdigest(),
        "normalized_post_load_fnv1a64": fnv(expected),
        "normalized_post_load_sha256": hashlib.sha256(expected).hexdigest(),
        "padding_offsets": list(PADDING),
        "padding_substitute": 255,
        "swap_operations": swap_count,
        "post_load_source_compared": post is not None,
        "passed": True,
    }
    if post is not None:
        if len(post) != SOURCE_SIZE:
            raise ValueError("post-load snapshot must contain exactly 17,824 bytes")
        normalized = bytearray(post)
        for offset in PADDING:
            normalized[offset] = 255
        mismatches = [index for index, (actual, wanted) in enumerate(zip(normalized, expected))
                      if actual != wanted]
        report.update({
            "observed_post_load_fnv1a64": fnv(post),
            "observed_post_load_sha256": hashlib.sha256(post).hexdigest(),
            "observed_normalized_fnv1a64": fnv(normalized),
            "mismatched_bytes": len(mismatches),
            "first_mismatch_offsets": mismatches[:32],
            "observed_changed_bytes_by_block": {
                tag: sum(source[index] != post[index] for index in range(at, at + size))
                for tag, (at, size) in blocks.items()
            },
            "passed": not mismatches,
        })
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path, help="Local original pumpkin RARC archive")
    source.add_argument("--source", type=Path, help="Local original pumpkin BMD")
    parser.add_argument("--post-load-source", type=Path, help="Optional local native source snapshot")
    parser.add_argument("--output", type=Path, help="Write metadata JSON to this path instead of stdout")
    args = parser.parse_args()
    if args.output and args.output.resolve() in {
            path.resolve() for path in (args.archive, args.source, args.post_load_source) if path}:
        parser.error("output must not overwrite an input")
    try:
        original = read_archive(args.archive) if args.archive else args.source.read_bytes()
        post = args.post_load_source.read_bytes() if args.post_load_source else None
        report = verify(original, post)
        output = json.dumps(report, indent=2) + "\n"
        if args.output:
            args.output.write_text(output, encoding="utf-8")
        else:
            print(output, end="")
        return 0 if report["passed"] else 1
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())
