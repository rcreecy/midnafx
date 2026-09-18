"""Check or apply MidnaFX's candidate patch to the pinned Dusklight host.

The default check is read-only. ``--apply`` changes only the local Dusklight
checkout; it is not part of the MidnaFX SDK package build.
"""

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PATCH = ROOT / "patches" / "dusklight-cpu-skinning-pc.patch"
PINNED_REVISION = "edf42c6a7202647b56dd2fcdef02d17671bc814b"


def git(checkout: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(checkout), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dusklight-dir", type=Path, default=ROOT / "upstream" / "dusklight")
    parser.add_argument("--apply", action="store_true", help="Apply patch after all checks pass")
    args = parser.parse_args()
    checkout = args.dusklight_dir.resolve()
    if not PATCH.is_file():
        parser.error(f"patch missing: {PATCH}")

    revision = git(checkout, "rev-parse", "HEAD")
    if revision.returncode or revision.stdout.strip() != PINNED_REVISION:
        parser.error(f"Dusklight must be at {PINNED_REVISION}: {revision.stderr.strip() or revision.stdout.strip()}")
    status = git(checkout, "status", "--porcelain", "--untracked-files=no")
    if status.returncode or status.stdout.strip():
        parser.error("Dusklight tracked files must be clean before checking or applying the patch")

    check = git(checkout, "apply", "--check", str(PATCH))
    if check.returncode:
        parser.error(f"candidate host patch does not apply: {check.stderr.strip()}")
    if args.apply:
        applied = git(checkout, "apply", str(PATCH))
        if applied.returncode:
            parser.error(f"could not apply host patch: {applied.stderr.strip()}")
        reverse = git(checkout, "apply", "--reverse", "--check", str(PATCH))
        if reverse.returncode:
            parser.error(f"applied patch did not verify: {reverse.stderr.strip()}")
        print(f"Applied candidate host patch to {checkout}")
    else:
        print(f"Candidate host patch applies to pinned Dusklight at {checkout}")


if __name__ == "__main__":
    main()
