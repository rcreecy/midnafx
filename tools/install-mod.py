"""Copy a MidnaFX .dusk package to an explicitly selected Dusklight mods folder."""

import argparse
import json
from pathlib import Path
import shutil
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--mods-dir", type=Path, required=True)
    parser.add_argument("--replace", action="store_true", help="Allow replacing an existing midnafx.dusk")
    args = parser.parse_args()
    package = args.package.resolve(strict=True)
    with zipfile.ZipFile(package) as archive:
        metadata = json.loads(archive.read("mod.json"))
        if metadata.get("name") != "MidnaFX":
            parser.error("Package metadata is not named MidnaFX.")
        if not any(name.startswith("lib/") and name.endswith((".dll", ".so")) for name in archive.namelist()):
            parser.error("Package does not contain a native mod library.")
    destination_dir = args.mods_dir.expanduser().resolve()
    destination = destination_dir / "midnafx.dusk"
    if destination == package:
        parser.error("Package is already in the selected destination.")
    if destination.exists() and not args.replace:
        parser.error(f"Destination exists: {destination}; use --replace to update it.")
    destination_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(package, destination)
    print(f"Installed {destination}. Use Reload in Dusklight's mod manager.")


if __name__ == "__main__":
    main()
