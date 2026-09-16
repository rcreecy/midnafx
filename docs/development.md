# Development and packaging

## Download a build

The [Build MidnaFX workflow](https://github.com/rcreecy/midnafx/actions/workflows/build.yml)
builds Windows AMD64 and Intel macOS packages on GitHub-hosted runners after pushes to
`main`, pull requests, or manual dispatch. Open a successful run, download the artifact
matching your platform, extract its `midnafx.dusk`, and copy that file to the Dusklight
mods folder listed below. Platform artifacts are separate; do not rename one platform's
package as a combined release. This removes the need for users to compile MidnaFX locally.
CI validates compilation and package structure, not game loading, Metal output, or latency.

MidnaFX uses the standalone Dusklight SDK at commit `edf42c6a7202647b56dd2fcdef02d17671bc814b`, including its pinned Aurora submodule `7f2801cd0133c9333eadb4e2e6b24100c328d328`. Research reference: official mod template commit `ece6d0dae843675fbd1e3f2308a0f1f782da5d80`. See `notes/render.md` for renderer contracts.

## Prerequisites and checkout

Use CMake 3.26+ (the Windows VS 2026 preset requires a CMake supporting that generator), Git, a C++20 compiler, and Python 3 for convenience scripts. On macOS use Xcode command-line tools and Ninja. The Intel target is `x86_64`; this is not a universal binary. A Windows build cannot validate Metal or produce the Intel Mac release by itself.

If the research checkout is absent:

```sh
git clone https://github.com/TwilitRealm/dusklight.git upstream/dusklight
git -C upstream/dusklight checkout edf42c6a7202647b56dd2fcdef02d17671bc814b
git -C upstream/dusklight submodule update --init extern/aurora
```

No full game build, game assets or ROM are needed to compile the mod SDK target. `DUSKLIGHT_DIR` can select another local checkout. CMake rejects a different HEAD unless `MIDNAFX_ALLOW_UNVERIFIED_SDK=ON` is explicitly selected for compatibility testing. That escape hatch is not a claim of compatibility; dirty checkout changes are also outside the reviewed source baseline.

The first native configure may download the pinned Dawn headers/package and the SDK's platform link stub. These are upstream SDK behavior, not vendored MidnaFX dependencies. `DUSK_GAME_EXE` selects a preexisting platform stub/game binary instead; Windows requires an import `.lib`. The stub download URL is controlled by upstream `DUSKLIGHT_SDK_STUB_URL`. Preserve acquired dependency versions/checksums with release records. M1 uses `FEATURES webgpu` only; it does not opt into the game ABI feature.

The pinned macOS x86_64 SDK link stub omits `LC_UUID`, which the current hosted Apple
linker requires for `-bundle_loader`. CI runs `tools/add_macho_uuid.py` after configure;
it adds a deterministic UUID in existing zero header padding without changing symbols.
If a local Intel Mac build reports a missing `LC_UUID`, run the same script against
`build/macos-intel-release/dusklight-sdk-stubs/stub-macos-x86_64` before building.

## Build

Windows with Visual Studio 2026 C++ workload:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel
```

Intel macOS (run on macOS, including an appropriate x86_64 toolchain):

```sh
cmake --preset macos-intel-release
cmake --build --preset macos-intel-release --parallel
```

Alternatively run `python tools/build-release.py --preset windows-release` or `--preset macos-intel-release`; `--cmake`, `--dusklight-dir` and `--game-link-stub` accept explicit paths. The script invokes subprocesses as argument arrays. On this development machine the bundled CMake is `C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`.

The SDK's `midnafx_package` target produces `build/<preset>/mods/midnafx.dusk`. It is a ZIP containing `mod.json`, `res/` resources, and a native library at `lib/<platform>-<architecture>/mod.dll` (Windows) or `mod.so` (other platforms, including macOS). It supports only the platform built. Do not present a Windows-only artifact as a Mac or combined release. `upstream/mod-template/tools/merge_mod.py` is the official reference for later combining independently validated architecture artifacts.

The passthrough and grading WGSL sources are embedded at configure time in a generated C++
header. Changing either shader triggers CMake reconfiguration; parameter tuning does not
recompile either pipeline. Unsupported formats/MSAA bypass the pass; a new supported
layout builds and retains its own pipelines until shutdown.

## Install and reload

Copy the package into the actual Dusklight user mods directory. Official template defaults:

- Windows: `%APPDATA%/TwilitRealm/Dusklight/mods`
- macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
- Linux: `~/.local/share/TwilitRealm/Dusklight/mods`

`python tools/install-mod.py build/windows-release/mods/midnafx.dusk --mods-dir "YOUR MODS DIRECTORY"` checks that the archive has MidnaFX metadata and a native library. Add `--replace` to update a previous package. The script does not run during a build and does not change the host executable or application bundle. Select **Reload** in the Dusklight mod manager after rebuilding/installing. Ensure architecture and host revision match.

For the Intel Mac, use the Mac package path and native user directory:

```sh
python3 tools/build-release.py --preset macos-intel-release
python3 tools/install-mod.py build/macos-intel-release/mods/midnafx.dusk \
  --mods-dir "$HOME/Library/Application Support/TwilitRealm/Dusklight/mods" --replace
```

Inspect `unzip -l build/macos-intel-release/mods/midnafx.dusk` and confirm
`lib/macos-x86_64/mod.so`. `file build/macos-intel-release/midnafx.so` (or the actual
module output path shown by CMake) must report x86_64 Mach-O. `ctest --preset
macos-intel-release` validates the package contract, not host execution. Use a Dusklight
build with the researched graphics service version; record its actual revision before
interpreting results.

## Validation

Portable test configuration avoids SDK/GPU dependencies: `cmake --preset tests`, `cmake --build --preset tests`, then `ctest --preset tests`. Test presets error when no tests are registered, so a successful empty run is never evidence of validation. `tests/CMakeLists.txt` is included when supplied. Native presets can run their tests with `ctest --preset <preset>`.
The portable `grade_math` test checks neutral parameters, exposure preparation, disabled
effects and bounded gamma values; it does not execute WGSL.

The current automated `package_contract` test checks manifest identity, native package
layout and binary architecture. On the Intel Mac, run this host checklist with a fixed save
and camera, keeping the same render resolution and time of day for each comparison:

1. Launch with MidnaFX installed but its **Enable passthrough** setting off. Verify the
   mod appears and its status is ready or a clear unsupported reason. Save a baseline frame.
2. Set MSAA to off, enable passthrough, and capture the same scene. Check orientation,
   corners, alpha-dependent composition, fades, menus and HUD. Toggle off and compare again.
   Compare screenshot pixel differences where the host permits a repeatable frame; account
   for animation and temporal bloom before calling any difference a regression.
3. Resize through 1080p, 1440p if available, and 4K. Aurora's pipeline layout key excludes
   dimensions, so resolution-only changes should keep the same pipeline and update the
   diagnostic size. A supported format change builds new pipelines; MSAA bypasses. Disable and reload
   the mod; check no error or GPU validation messages. Repeat with MSAA enabled and verify
   **Unsupported scene layout or MSAA; bypassed** before any M1 draw.
4. Repeat with Dawnlight and a high-resolution texture pack active. Compare Vanilla,
   Dawnlight only, Dawnlight plus M1 disabled, and Dawnlight plus M1 enabled at the same
   location. Check ordinary HUD and Dawnlight HUD layouts separately.
5. Record CPU stage callback time from M1 diagnostics, host GPU capture or platform GPU
   profiler timings for snapshot copy and fullscreen draw, and enabled/disabled total frame
   timing distributions. Save exact Mac model/GPU, macOS version, Dusklight and Dawnlight
   revisions, backend, bloom mode, resolution, and capture procedure with results in
   `docs/performance.md`. Do not treat M1 counters as GPU timing.

If a shader or pipeline validation error appears, leave passthrough disabled and retain
the log and host revision for diagnosis. A successful package build does not prove
Metal execution or image parity.

A successful compile/package is not host runtime validation. Verify startup, enable/disable, neutral image identity, HUD exclusion, resize, stage changes, reload and shutdown in an actual host. For M1 test both successful single-sample rendering and clean MSAA bypass. Record host revision, backend, architecture, render resolution, texture packs and Dawnlight version. Intel Mac/Metal 4K performance remains a separate required hardware check.

For M2, first use **Force passthrough comparison** with **Enable grading** to repeat the M1
neutral test. Then turn passthrough off: all neutral sliders should produce the disabled
image with no submitted MidnaFX draw. Adjust one control at a time and verify direction,
range, persistence across restart, per-effect toggle, and Restore neutral grading. Repeat
with a changing Twilight scene and inspect for clipping/banding and unintended HUD changes.
The Windows package test does not compile WGSL on the target Metal backend.

For M3, select **Vanilla** for the neutral built-in look. **Custom** is the live edited look.
**Save current preset** creates a numbered saved copy, or updates a selected saved copy;
**Duplicate current look** creates another numbered copy. **Load selected preset** restores
its stored values and effect toggles. Editing a control selects Custom. Up to 16 saved
presets are persisted through Dusklight ConfigService. Test Save, Load, Duplicate,
restart persistence, and malformed config recovery in the host UI. The portable
`presets_roundtrip` test covers the versioned storage format and rejected corrupt input.

Source evidence for build decisions: `upstream/dusklight/sdk/CMakeLists.txt`; `cmake/ModSDK.cmake::{add_mod,_mod_lib_info,_mod_add_webgpu_headers,_mod_download_link_stub}`; `upstream/mod-template/{CMakeLists.txt,README.md}` at the revisions above. The SDK creates a module library, C++20 requirement, hidden exports, platform-specific host linking and a `.dusk` packaging target. MidnaFX delegates those details to the supported helper.
