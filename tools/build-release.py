"""Configure and build one native package using the checked-in CMake presets."""

import argparse
import platform
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake", help="CMake executable path")
    parser.add_argument("--preset", choices=("windows-release", "macos-intel-release", "tests"))
    parser.add_argument("--dusklight-dir", type=Path)
    parser.add_argument("--game-link-stub", type=Path)
    args = parser.parse_args()
    preset = args.preset
    if preset is None:
        preset = {"Windows": "windows-release", "Darwin": "macos-intel-release"}.get(platform.system())
        if preset is None:
            parser.error("Choose --preset tests or configure a native Linux build manually.")
    root = Path(__file__).resolve().parents[1]
    configure = [args.cmake, "--preset", preset]
    if args.dusklight_dir:
        configure.append(f"-DDUSKLIGHT_DIR={args.dusklight_dir.resolve()}")
    if args.game_link_stub:
        configure.append(f"-DDUSK_GAME_EXE={args.game_link_stub.resolve()}")
    subprocess.run(configure, cwd=root, check=True)
    subprocess.run([args.cmake, "--build", "--preset", preset, "--parallel"], cwd=root, check=True)
    if preset != "tests":
        package = root / "build" / preset / "mods" / "midnafx.dusk"
        if not package.is_file():
            raise SystemExit(f"Build finished without expected package: {package}")
        print(f"Built native package: {package}")


if __name__ == "__main__":
    main()
