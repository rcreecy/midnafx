"""Check, apply, or reverse the pinned Dusklight decompressor shutdown repair.

The default check is read-only. Unrelated local patches are preserved; only
this patch's forward and reverse applicability determine its current state.
This does not rebuild or modify packaged MidnaFX binaries.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = 'edf42c6a7202647b56dd2fcdef02d17671bc814b'
PATCH = ROOT / 'patches/dusklight-decomp-shutdown.patch'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dusklight-dir', type=Path, default=ROOT / 'upstream/dusklight')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    checkout = args.dusklight_dir.resolve()
    if not PATCH.is_file():
        parser.error(f'Patch missing: {PATCH}')

    def git(*arguments):
        return subprocess.run(['git', '-C', str(checkout), *arguments],
                              capture_output=True, text=True, check=False)

    revision = git('rev-parse', 'HEAD')
    if revision.returncode or revision.stdout.strip() != PIN:
        parser.error(f'Dusklight must be pinned to {PIN}')
    forward = git('apply', '--check', str(PATCH))
    reverse = git('apply', '--reverse', '--check', str(PATCH))
    if forward.returncode and reverse.returncode:
        parser.error('Patch neither applies nor reverses cleanly; preserve existing edits and inspect conflicts')
    if not forward.returncode and not reverse.returncode:
        parser.error('Patch state is ambiguous; preserve existing edits and inspect the source')
    if args.apply and not forward.returncode:
        result = git('apply', str(PATCH))
        verification = ('apply', '--reverse', '--check', str(PATCH))
    elif args.reverse and not reverse.returncode:
        result = git('apply', '--reverse', str(PATCH))
        verification = ('apply', '--check', str(PATCH))
    else:
        print('Shutdown repair is already applied' if not reverse.returncode
              else 'Shutdown repair is absent and applies cleanly')
        return
    if result.returncode:
        parser.error(result.stderr.strip())
    verified = git(*verification)
    if verified.returncode:
        parser.error(f'Patch operation did not verify: {verified.stderr.strip()}')
    print('Shutdown repair applied' if args.apply else 'Shutdown repair reversed')


if __name__ == '__main__':
    main()
