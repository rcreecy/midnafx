"""Reject unsupported fixture arguments before acquiring a GPU."""
import subprocess
import sys

for arguments in (['--surface-maps-only'], ['--unknown'], ['--exposure-only', 'extra']):
    result = subprocess.run([sys.argv[1], *arguments], text=True, capture_output=True, timeout=10)
    if result.returncode != 2 or 'Unsupported option' not in result.stderr:
        raise RuntimeError(f'argument validation failed for {arguments}: exit={result.returncode}, stderr={result.stderr}')
print('Unsupported pixel-test arguments fail explicitly before GPU setup.')
