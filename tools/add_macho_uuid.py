"""Add a deterministic LC_UUID to the SDK's x86_64 Mach-O link stub.

The pinned Dusklight SDK stub omits this command; current Apple linkers reject it as
the -bundle_loader input. Only pre-existing zero header padding is touched.
"""

import hashlib
from pathlib import Path
import struct
import sys


def repair(path: Path) -> None:
    data = bytearray(path.read_bytes())
    if len(data) < 64 or struct.unpack_from("<II", data) != (0xFEEDFACF, 0x01000007):
        raise ValueError("expected an x86_64 Mach-O 64-bit link stub")
    command_count, command_bytes = struct.unpack_from("<II", data, 16)
    end = 32 + command_bytes
    if end > len(data) or command_count > 256:
        raise ValueError("invalid Mach-O load commands")
    offset = 32
    for _ in range(command_count):
        if offset + 8 > end:
            raise ValueError("truncated Mach-O load command")
        command, size = struct.unpack_from("<II", data, offset)
        if size < 8 or offset + size > end:
            raise ValueError("invalid Mach-O load command size")
        if command == 0x1B:
            return
        offset += size
    if offset != end or data[end : end + 24] != bytes(24):
        raise ValueError("no empty header padding for LC_UUID")
    digest = hashlib.sha256(data).digest()[:16]
    struct.pack_into("<II16s", data, end, 0x1B, 24, digest)
    struct.pack_into("<II", data, 16, command_count + 1, command_bytes + 24)
    path.write_bytes(data)


if __name__ == "__main__":
    repair(Path(sys.argv[1]))
