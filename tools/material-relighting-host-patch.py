"""Check, apply or reverse the optional native material relighting host extension."""

import argparse
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parent.parent
HOST_REVISION = 'edf42c6a7202647b56dd2fcdef02d17671bc814b'
AURORA_REVISION = '7f2801cd0133c9333eadb4e2e6b24100c328d328'


def git(checkout, *args):
    return subprocess.run(['git', '-C', str(checkout), *map(str, args)],
                          text=True, capture_output=True, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dusklight-dir', type=Path, default=ROOT/'upstream/dusklight')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    host = args.dusklight_dir.resolve()
    components = [
        (host, HOST_REVISION, ROOT/'patches/dusklight-material-relighting.patch'),
        (host/'extern/aurora', AURORA_REVISION, ROOT/'patches/aurora-material-relighting.patch'),
    ]
    if args.reverse:
        components.reverse()
    direction = ['--reverse'] if args.reverse else []
    for checkout, revision, patch in components:
        result = git(checkout, 'rev-parse', 'HEAD')
        if result.returncode or result.stdout.strip() != revision:
            parser.error(f'{checkout} must be pinned to {revision}')
        result = git(checkout, 'apply', '--check', *direction, patch)
        if result.returncode:
            parser.error(f'{patch.name} does not apply: {result.stderr.strip()}')
    if not (args.apply or args.reverse):
        print('Native material relighting patches apply to the pinned host and Aurora.')
        return

    applied = []
    for checkout, _, patch in components:
        result = git(checkout, 'apply', *direction, patch)
        if result.returncode:
            for previous_checkout, previous_patch in reversed(applied):
                rollback = git(previous_checkout, 'apply', *([] if args.reverse else ['--reverse']),
                               previous_patch)
                if rollback.returncode:
                    parser.error(f'patch application and rollback failed: {rollback.stderr.strip()}')
            parser.error(result.stderr.strip())
        applied.append((checkout, patch))
    print(f'Native material relighting {"removed from" if args.reverse else "applied to"} {host}.')


if __name__ == '__main__':
    main()
