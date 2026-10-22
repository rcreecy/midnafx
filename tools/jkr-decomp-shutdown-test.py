"""Compile the real pinned decompressor and test bounded shutdown on Windows.

The queue/thread seam controls scheduling; it does not replace JKRDecomp code.
An unrepaired full-queue failure is reported with exit status 1, after recovery.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--host-build', type=Path, default=Path('build/runtime-host'))
    parser.add_argument('--output', type=Path, default=Path('build/remaster-r0/decomp-shutdown'))
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This qualification harness requires the Windows host toolchain')
    root = args.root.resolve()
    host_build = (root / args.host_build).resolve()
    output = (root / args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    project = host_build / 'JSystem_JKernel.vcxproj'
    namespace = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    project_tree = ET.parse(project)
    group = next(g for g in project_tree.findall('m:ItemDefinitionGroup', namespace)
                 if 'RelWithDebInfo|x64' in g.attrib.get('Condition', ''))
    compile_group = group.find('m:ClCompile', namespace)
    includes = compile_group.find('m:AdditionalIncludeDirectories', namespace).text.split(';')
    definitions = compile_group.find('m:PreprocessorDefinitions', namespace).text.split(';')
    # This project emits its definitions twice for MSBuild. Select the form without
    # shell backslashes; response-file quoting is handled by the compiler itself.
    definitions = [value.replace('\\"', '"') for value in definitions
                   if value and not value.startswith('%(')]
    source = root / 'upstream/dusklight/libs/JSystem/src/JKernel/JKRDecomp.cpp'
    harness = root / 'tools/qualification/jkr_decomp_shutdown.cpp'
    executable = output / 'jkr-decomp-shutdown.exe'
    compiler_args = ['/nologo', '/std:c++20', '/EHsc', '/MD', '/O2', '/Gy', '/Gw', '/utf-8']
    compiler_args += ['/I"' + value + '"' for value in includes
                      if value and not value.startswith('%(')]
    compiler_args += ['/D' + value for value in definitions]
    compiler_args += [f'"{source}"', f'"{harness}"', f'/Fo"{output}/"',
                      f'/Fe"{executable}"', '/link /OPT:REF /OPT:ICF']
    response = output / 'compile.rsp'
    response.write_text('\n'.join(compiler_args), encoding='utf-8')
    setup = Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat')
    if not setup.is_file():
        parser.error('The configured Visual Studio 18 Community toolchain is unavailable')
    batch = output / 'compile.cmd'
    batch.write_text(f'@call "{setup}" -no_logo -arch=x64\n@if errorlevel 1 exit /b %errorlevel%\n'
                     f'@cl @"{response}"\n', encoding='utf-8')
    build = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=root,
                           capture_output=True, text=True, timeout=180)
    (output / 'compile.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode:
        print(f'Compilation failed; see {output / "compile.log"}', file=sys.stderr)
        return build.returncode
    results = []
    for case in ('empty', 'full'):
        trial = subprocess.run([str(executable), case], cwd=root, capture_output=True,
                               text=True, timeout=12)
        (output / f'{case}.log').write_text(trial.stdout + trial.stderr, encoding='utf-8')
        result = json.loads(trial.stdout)
        result['exit_code'] = trial.returncode
        results.append(result)
    summary = {'source': source.relative_to(root).as_posix(),
               'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
               'harness_sha256': hashlib.sha256(harness.read_bytes()).hexdigest(),
               'compiler_project': project.relative_to(root).as_posix(),
               'limitations': ['The OS queue/thread functions are controlled test seams.',
                               'This test does not attribute the historical baseline stall.'],
               'cases': results}
    (output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
