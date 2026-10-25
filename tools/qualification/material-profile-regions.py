"""Measure named RGB rectangles in unchanged material-profile screenshots.

Requires Pillow. The manifest supplies image filenames, rectangles [x,y,w,h],
and comparison pairs. Images are decoded without resizing or registration;
only metadata is written. Results describe these rectangles, not whole frames.
"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def measure(manifest_path):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    root = manifest_path.resolve().parent
    frames, captures = {}, {}
    for name, filename in manifest["frames"].items():
        path = (root / filename).resolve()
        if path.parent != root:
            raise ValueError("frame images must be beside the manifest")
        with Image.open(path) as image:
            frames[name] = image.convert("RGB")
        captures[name] = {"file": filename, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                          "size": list(frames[name].size)}
    if not frames or len({image.size for image in frames.values()}) != 1:
        raise ValueError("frames must have identical dimensions")
    width, height = next(iter(frames.values())).size
    for rectangle in manifest["regions"].values():
        if len(rectangle) != 4 or any(type(value) is not int for value in rectangle):
            raise ValueError("rectangles require four integer values")
        x, y, w, h = rectangle
        if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
            raise ValueError("rectangle exceeds image bounds")
    comparisons = {}
    for name, pair in manifest["comparisons"].items():
        first, second = frames[pair["first"]], frames[pair["second"]]
        regions = {}
        for label, (x, y, w, h) in manifest["regions"].items():
            box = (x, y, x + w, y + h)
            before, after = first.crop(box).tobytes(), second.crop(box).tobytes()
            differences = [abs(a - b) for a, b in zip(before, after)]
            pixels = w * h
            regions[label] = {
                "pixels": pixels,
                "mean_abs_rgb": sum(differences) / (pixels * 3),
                "max_abs_channel": max(differences),
                "changed_pixels": sum(before[i:i + 3] != after[i:i + 3]
                                      for i in range(0, len(before), 3)),
                "mean_rgb_delta_second_minus_first": [
                    sum(after[i] - before[i] for i in range(channel, len(before), 3)) / pixels
                    for channel in range(3)
                ],
            }
        comparisons[name] = {**pair, "regions": regions}
    return {
        "schema": 1,
        "metric": "Decoded RGB channels on the 0-255 scale; no image resizing or registration",
        "scope": "Named rectangles only; whole-frame equality and motion quality are not established",
        "captures": captures,
        "regions": manifest["regions"],
        "comparisons": comparisons,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="Metadata JSON output; defaults to stdout")
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        inputs = {args.manifest.resolve()} | {
            (args.manifest.resolve().parent / filename).resolve()
            for filename in manifest["frames"].values()
        }
        if args.output and args.output.resolve() in inputs:
            raise ValueError("output must not overwrite an input")
        output = json.dumps(measure(args.manifest), indent=2) + "\n"
        if args.output:
            args.output.write_text(output, encoding="utf-8")
        else:
            print(output, end="")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
