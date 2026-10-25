# Owned native fixture qualification, 2026-10-24

REA/Ghidra analyzes the built Windows x86-64 PE fixture and a bounded Frida
observation confirms its arithmetic on an owned process. This qualifies a small
native-analysis/runtime-observation path on this host. It does not qualify
instrumentation of Dusklight, MidnaFX, a game scene, or macOS.

## Identity and retained records

- [Fixture source](../../../tools/qualification/rea_fixture.cpp), SHA256
  `dd3a9350ff86ea5f05c6bc9ef975f903ef5390de08275f852faab3473003055d`.
- [Observation script](../../../tools/qualification/rea_fixture_probe.js), SHA256
  `a921d842a098927c30a0601cf594d9b131dc9f9e4a2b906f954da70a612b9ff2`.
- Local `build/windows-release/tests/Release/rea_fixture.exe`, SHA256
  `73ec568a0472b39c2921708ef9e2c93d66cc09badc89a65e39dc1f592c2ed994`.
  Its current bytes match the PE artifact digest in the exported bundle.
- Unmodified [REA evidence bundle](evidence.json), SHA256
  `c4c1d9b9a7acc8948d690955f0d10ce26255277e938f996898eced66dc769de1`.
  [Summary metadata](summary.json) records the hashes, sample values and cleanup
  provenance. No fixture executable is included here.

The optional local snapshot is not duplicated. Its SHA256 is
`16391dc299bf0881f312de7bd065a57dc079d4a293410e80a7ec714684165a7d`;
the retained bundle already contains the selected function analysis and runtime
observation. Provider limitations remain intact in the bundle.

## Observed behavior

Ghidra 12.1.2 analyzes `midnafx_rea_probe` at preferred-image address
`0x140001140`. Its assembly performs multiply-by-three, XOR `0x5a17`, add seven,
then return. The recovered pseudocode agrees with the source arithmetic:

```cpp
return ((input * 3u) ^ 0x5a17u) + 7u;
```

The actual source uses unsigned 32-bit values. Ghidra's recovered `int`
declaration is an analysis artifact, not a correction to the source declaration.
The fixture's uninstrumented default check expects `input=7 result=23049`.
The retained runtime samples include that same input/result.

Ghidra Evidence ID:
`ev_12599dbee109483734004ecc58976cf63e9bc82bd8d2b9b8c6f33636b9677cec`.

Frida spawns the owned fixture with `--serve-ms 5000`. The script observes
arguments and returns without replacing them. Its first eight sample records,
inputs 0 through 7, each match independently calculated arithmetic. Its bounded
summary reports **78 observed calls and zero mismatches**. This is the script's
observation-window count, not a count of every call during the process lifetime.
Messages are not truncated. The Frida version is not reported.

Frida Evidence ID:
`ev_03a6a84f10c5b01fc69cb4b747b47c2336488023c84597eea4a6730dd7f8ecf3`.

## Cleanup and limitations

The exported Frida observation says `state: running` and `cleanup_error: null`.
Neither field proves detachment. The subsequent parent-task tool observations
report no open session for exact ID
`6658ed94-0108-459d-907a-9c80f1861519` and PID 7892 absent within an eight-second
follow-up wait. These separate observations establish session retirement and
process disappearance. Their raw responses are not included in the exported
bundle; the provenance is explicit in the summary. No process exit code was
retained, and no particular process-termination mechanism is inferred.

The Frida evidence record itself does not provide artifact identity, operating
system or architecture. Its target path and script identity are retained; the
binary hash and Windows x86-64 PE identity come from the separate static artifact
record and local hash verification. Do not relabel that missing runtime identity
as a measured Frida capability.

The fixture performs no game, UI, network or file operations. This evidence does
not measure instrumentation overhead, prove arbitrary function/type recovery,
establish whole-process allocation coverage, or provide a PE dependency audit.
