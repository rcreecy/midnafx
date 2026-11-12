"""Qualify a shipped surface-map payload; explicitly skip an absent experiment."""
from pathlib import Path
import sys


def validate(patch, files, marker='surface_maps::'):
    chunks = {}
    for target in files:
        matches = [part for part in patch.split('diff --git ')
                   if part.startswith('a/'+target+' b/'+target+'\n')]
        if len(matches) > 1:
            raise ValueError('duplicate payload: '+target)
        if matches: chunks[target] = matches[0]
    if not chunks and not any(Path(target).name in patch for target in files) and marker not in patch:
        return False
    if set(chunks) != set(files):
        raise ValueError('surface-map payload is partial or referenced without its files')
    for target, expected in files.items():
        chunk = chunks[target]
        if 'new file mode 100644' not in chunk:
            raise ValueError('expected a new-file payload: '+target)
        added = '\n'.join(line[1:] for line in chunk.splitlines()
                          if line.startswith('+') and not line.startswith('+++'))+'\n'
        if added != expected:
            raise ValueError('payload differs from tested implementation: '+target)
    return True


def main():
    root = Path(sys.argv[1])
    patch = (root/'patches/aurora-material-relighting.patch').read_text()
    files = {'lib/gfx/'+name: (root/'src/render'/name).read_text()
             for name in ('surface_map_cache.hpp', 'surface_map_shader.hpp')}
    try:
        shipped = validate(patch, files)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    if not shipped:
        print('SKIP: optional host patch ships no surface-map payload; standalone fixtures do not prove host integration.')
        return 77
    print('Surface map cache and GPU kernel match the shipped host patch.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
