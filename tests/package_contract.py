"""Check the native package layout and binary target, independent of Dusklight runtime."""

import json
from pathlib import Path
import struct
import sys
import zipfile


archive_path = Path(sys.argv[1])
assert archive_path.is_file(), f"missing package: {archive_path}"
with zipfile.ZipFile(archive_path) as package:
    names = set(package.namelist())
    manifest = json.loads(package.read("mod.json"))
    assert manifest["id"] == "com.midnafx.midnafx"
    assert manifest["version"] == "0.2.0"
    native = [name for name in names if name.startswith("lib/") and name.endswith(("mod.dll", "mod.so"))]
    assert len(native) == 1, native
    binary = package.read(native[0])
    platform = native[0].split("/")[1]
    if platform == "windows-amd64":
        assert binary[:2] == b"MZ"
        pe = struct.unpack_from("<I", binary, 0x3C)[0]
        assert binary[pe:pe + 4] == b"PE\0\0"
        assert struct.unpack_from("<H", binary, pe + 4)[0] == 0x8664
    elif platform == "macos-x86_64":
        magic = binary[:4]
        assert magic in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf"), magic
        little = magic == b"\xcf\xfa\xed\xfe"
        assert struct.unpack_from("<I" if little else ">I", binary, 4)[0] == 0x01000007
    else:
        assert binary, platform
print(f"Valid {platform} native package: {archive_path}")
