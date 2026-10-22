"""Check, apply, or reverse bounded CPU frame telemetry on pinned Aurora.

Only frame.cpp changes. Set MIDNAFX_FRAME_TIMING_FILE before launching the host
to record CPU wall timestamps and successful present-call completion intervals.
The file is written only after the render worker joins during graphics shutdown.
This patch does not measure GPU, encoding, allocation, or physical scanout time.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = '7f2801cd0133c9333eadb4e2e6b24100c328d328'
PATCH = ROOT / 'patches/aurora-frame-timing.patch'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aurora-dir', type=Path, default=ROOT / 'upstream/dusklight/extern/aurora')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    checkout = args.aurora_dir.resolve()
    if not PATCH.is_file():
        parser.error(f'Patch missing: {PATCH}')

    def git(*arguments):
        return subprocess.run(['git', '-C', str(checkout), *arguments],
                              capture_output=True, text=True, check=False)

    revision = git('rev-parse', 'HEAD')
    if revision.returncode or revision.stdout.strip() != PIN:
        parser.error(f'Aurora must be pinned to {PIN}')
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
        print('Frame timing patch is already applied' if not reverse.returncode
              else 'Frame timing patch is absent and applies cleanly')
        return
    if result.returncode:
        parser.error(result.stderr.strip())
    verified = git(*verification)
    if verified.returncode:
        parser.error(f'Patch operation did not verify: {verified.stderr.strip()}')
    print('Frame timing patch applied' if args.apply else 'Frame timing patch reversed')


if __name__ == '__main__':
    main()
