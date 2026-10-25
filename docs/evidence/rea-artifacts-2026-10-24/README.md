# Windows artifact/import baseline, 2026-10-24

This read-only baseline checks the current Windows package against the selected
qualification host. It uses fixed ZIP-member extraction, the existing package
validator and Microsoft `dumpbin` 14.50.35722.0. No game process is launched or
instrumented, and no complete host decompilation is performed.

## Exact inputs

| Artifact | SHA256 | Size |
|---|---|---|
| `build/windows-release/mods/midnafx.dusk` | `01582539f1f2a8dd1fec367e0680974de3a975521a4c1e17a44393e804120d2a` | 154,356 bytes |
| Fixed member `lib/windows-amd64/mod.dll` | `e48920f2aef5726d6f1bc3dbee7ebecffc3291e2ad59467e99daadfd565faeaf` | 360,448 bytes |
| `build/frame-timing/hosts/baseline/dusklight.exe` | `444d8ee33ed78ba4a427810e35a62731f4d845cbac7343b37b16a5c20f3c34a4` | 51,795,456 bytes |

The package is the final local warm-profile build after the logging-only toggle
correction. The audit re-extracts it after that rebuild; earlier temporary
candidate output is not retained as this baseline. Package version is `1.2.0`;
this is not a claim that a new release was published. The package SHA256 is
checked again after the audit to detect concurrent changes.

`CMakeLists.txt:7` declares Dusklight revision
`edf42c6a7202647b56dd2fcdef02d17671bc814b`. That revision's Aurora gitlink is
`7f2801cd0133c9333eadb4e2e6b24100c328d328`; both current checkout HEADs match.
Those pins describe the available source checkouts. They do not reconstruct the
selected host binary or establish that every local qualification patch is absent.
The directory name `baseline` is not evidence of an unmodified host.

## Static observations

The existing package validator passes the fixed package and version:

```powershell
python tests/package_contract.py build/windows-release/mods/midnafx.dusk 1.2.0
```

Both native inputs have PE signature, AMD64 machine `0x8664` and PE32+ optional
header `0x20b`. The module exports exactly `mod_ctx`, `mod_initialize`, `mod_meta`,
`mod_shutdown` and `mod_update`.

The module imports ten DLL/image names, including the expected literal
**`dusklight.exe`**. All **80 named imports from that image** occur in the
selected host's 30,033-name export inventory; no selected-host import is missing.
Of those imports, **34 WebGPU names are forwarded** to same-named
`webgpu_dawn` exports. Each target name also occurs in the selected runtime's
`webgpu_dawn.dll` export inventory. These checks preserve the native host naming
contract that the earlier renamed-host experiment violated.

The module's direct import names are `dusklight.exe`, `MSVCP140.dll`,
`VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, `KERNEL32.dll` and five
`api-ms-win-crt-*` contracts. The host imports 32 DLL names. The manifest records
every direct import name, counts of named/ordinal imports, selected local
candidate scope/hash and missing-export results.

Export lookup succeeds for all directly checked candidates: five module
dependencies and 21 host dependencies, using the selected runtime directory
or this machine's Windows System32. This is an export-presence check, not proof
that the OS loader selects those files. Five module and eleven host API-set
contracts have no direct local DLL file checked here. They remain **unverified**,
not classified as missing exports. Recursive forwarded dependencies and API-set
mapping are not fully resolved.

## Retained evidence and limits

[manifest.json](manifest.json) records the binary/package hashes, identities,
import checks, forwarded names, source pins, validator result and text hashes.
The retained `module-*` headers/imports/exports and `host-*` headers/imports are
the actual `dumpbin` outputs. The 3.7 MB complete host export listing remains
ignored under `build/rea/artifact-baseline-r1`; its SHA256 and relevant matching
names are retained instead. No module, host, runtime DLL or game payload is
included in this evidence directory.

Reproduction uses `dumpbin /headers`, `/imports` and `/exports` on each selected
PE file, then exact imported-name/ordinal lookup in the chosen provider's exports.
Only `lib/windows-amd64/mod.dll` is extracted from the ZIP into ignored scratch.
The full archive is never extracted.

This does not prove ABI compatibility, complete dependency closure, runtime
loading, loaded-module identity, game rendering or performance. Other forwarded
system imports may require additional provider resolution. The existing launcher
and runtime proof remain necessary. No macOS artifact, macOS loader or Metal
execution is examined; macOS support remains best effort.

REA's `inspect_native_load_image` is limited to DOS MZ, COM and raw images and is
not used as a PE import auditor. This baseline uses the existing Windows tools
alongside the separately retained owned-fixture REA qualification.
