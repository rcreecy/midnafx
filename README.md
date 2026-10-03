# MidnaFX

**A native visual enhancement mod for The Legend of Zelda: Twilight Princess on Dusklight.**

[![Build MidnaFX](https://github.com/rcreecy/midnafx/actions/workflows/build.yml/badge.svg)](https://github.com/rcreecy/midnafx/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

MidnaFX 1.0 brings configurable color grading, subtle detail enhancement, saved looks, Twilight-aware profiles, and optional camera and geometry improvements to Hyrule. Start with **Natural / Vivid Realism** for restrained color, lifted midtones, and softer highlights, or build your own look with live controls. Color processing runs before the HUD, keeping interface elements out of the grade.

This is a C++20 / WebGPU mod loaded by **Dusklight**, not a standalone game or an emulator shader pack. This repository contains the native mod, WGSL shaders, host extension patches, tests, and development evidence. It includes no game assets or game image; playing requires a working Dusklight installation and your own game data.

**[Download](https://github.com/rcreecy/midnafx/releases/latest)** · **[v1.0 release notes](docs/releases/v1.0.0.md)** · **[Build guide](docs/development.md)** · **[Report an issue](https://github.com/rcreecy/midnafx/issues)**

## Features

| Feature | What it does |
| --- | --- |
| **Color grading** | Exposure, black point, contrast, gamma, saturation, highlight rolloff, temperature, and tint in a fused pre-HUD pass. Neutral grading with detail disabled bypasses that pass. |
| **Detail enhancement** | Optional five-tap detail processing with adjustable strength, integrated into the grading shader. |
| **Presets** | Built-in Vanilla and Natural / Vivid Realism, a separately retained Custom look, and up to 16 saved presets with save, load, duplicate, and restart persistence. |
| **Twilight profiles** | Opt-in blending from the general look toward a captured target when the game reports active Twilight. Twilight-spot state intentionally retains the general look. |
| **Exploration camera** | Independent wider vertical FOV and lower chase-camera controls. Opt-in modifiers apply only to supported exploration output and yield to authored events, first person, aiming, lock-on, special cameras, and detached views. Requires the host extension below. |
| **Adaptive normal smoothing** | Experimental, default-off smoothing for a small exact allowlist of validated rigid and skinned models. Unsupported resources are left unchanged; original normals are restored when disabled or unloaded. |
| **Depth of field** | Experimental, default-off near/far blur with a 2–12 pixel radius, manual focus, optional smooth camera-target autofocus, and depth-aware compositing before the HUD. |
| **Visual diagnostics** | Forced RGBA passthrough, A/B split, luminance, highlight/shadow clipping, and amplified difference views using the same scene snapshot. |
| **CPU diagnostics** | Stage p50/p95 timings, layout/resolve call times, snapshot requests, and pipeline counts for repeatable development comparisons. These are not GPU timings. |

See the [Natural / Vivid Realism guide](docs/vivid-realism.md) for exact preset settings and the [architecture guide](docs/architecture.md) for rendering contracts and limits.

## Install and get started

1. Download the `.dusk` asset matching your platform from [GitHub Releases](https://github.com/rcreecy/midnafx/releases/latest).
2. Copy the `.dusk` file itself into Dusklight's user mods folder:

   | Platform | Release asset | Mods folder |
   | --- | --- | --- |
   | Windows x64 | `midnafx-windows-amd64.dusk` | `%APPDATA%\TwilitRealm\Dusklight\mods` |
   | Intel macOS | `midnafx-macos-x86_64.dusk` | `~/Library/Application Support/TwilitRealm/Dusklight/mods` |

3. Start Dusklight, or choose **Reload** in its mod manager after replacing an installed package. Open MidnaFX's settings and turn on **Enable grading**.
4. Choose **Presets > Current preset > Natural / Vivid Realism**. Save or duplicate the look to make an editable copy. Select **Vanilla** to restore neutral grading; camera, geometry, and depth-of-field controls are independent.

Each package contains one platform's native library. The Intel Mac package is x86_64, not a universal or native Apple Silicon build. No Linux release package is provided by the current CI workflow. Development artifacts are also available from successful [build runs](https://github.com/rcreecy/midnafx/actions/workflows/build.yml); extract their artifact ZIP to obtain `midnafx.dusk`.

### Compatibility and host requirements

MidnaFX is developed against Dusklight revision `edf42c6a7202647b56dd2fcdef02d17671bc814b` and its pinned Aurora dependency. Compatibility with arbitrary host revisions is not established.

- **Exploration camera:** requires MidnaFX's CameraService 1.3 host patch. On a host without the extension, the feature reports unavailable.
- **Camera-target autofocus:** requires the CameraService 1.4 extension. Manual focus uses the base camera service.
- **Rendering:** unsupported scene layouts or MSAA bypass the grading pass. Start with MSAA off when validating the render path and inspect the in-mod status.
- **Host patches:** installing a `.dusk` package does not patch the Dusklight executable. See [camera integration](docs/camera-investigation.md), [depth-of-field integration](docs/depth-of-field-investigation.md), and the [build guide](docs/development.md) before building a source-matched host.

## v1 validation and known limits

The Windows v1 engineering gates are complete. Recorded D3D11 and D3D12 host testing covers rendering, camera transitions, resource restoration, and lifecycle behavior. The final combined D3D12 pass ran grading, detail, exploration camera, depth of field, and allowlisted static smoothing together at 1216×896, then shut down cleanly with original normal arrays restored. The v1 release record reports all 13 native-suite tests passing, including shader and package checks.

The release boundary remains explicit:

- **Intel Mac/Metal:** CI builds and checks the package, but live rendering and GPU performance have not been validated on Intel Mac hardware.
- **Depth of field:** the final silhouette correction has runtime execution evidence, but a fresh matched screenshot of that correction remains outstanding.
- **Twilight:** normal and active-Twilight behavior have live Windows coverage; Twilight-spot behavior has deterministic coverage without a live state-2 capture.
- **Geometry:** smoothing is limited to exact validated resource fingerprints, not a global model upgrade. Both rigid and skinned controls remain default off.
- **Scope:** native fog/bloom/lighting overrides, subdivision, mesh or material replacement, and broader model allowlists are outside v1.

Read the [v1 readiness audit](docs/v1-readiness.md) and [release correctness review](docs/notes/v1-release-correctness-review.md) for the evidence behind these claims. A successful build is not proof of image quality, platform parity, or GPU performance.

## Build from source

Prerequisites: Git, Python 3, CMake 3.26+, and a C++20 compiler. The Windows preset uses Visual Studio 2026 and needs a CMake version supporting that generator; the Intel macOS preset uses Xcode command-line tools and Ninja.

```sh
git clone https://github.com/rcreecy/midnafx.git
cd midnafx
git clone https://github.com/TwilitRealm/dusklight.git upstream/dusklight
git -C upstream/dusklight checkout edf42c6a7202647b56dd2fcdef02d17671bc814b
git -C upstream/dusklight submodule update --init extern/aurora
python tools/camera-host-patch.py --dusklight-dir upstream/dusklight --apply
```

The patch step matches CI's SDK setup; it does not build or install a patched game host. Then build and verify the Windows package:

```sh
cmake --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release
```

On Intel macOS, use `macos-intel-release` for each preset and `python3` where needed. Follow the [macOS link-stub instructions](docs/development.md) if the linker requires the upstream stub's UUID repair. The package is written to `build/<preset>/mods/midnafx.dusk`.

Compiling the SDK mod target needs no game image or full game build. Initial configuration may download upstream SDK dependencies. The build guide covers custom SDK paths, toolchain setup, packaging, installation scripts, and host patch handling.

## How development is verified

Verification combines deterministic tests, shader/package checks, and controlled in-game comparisons. Each answers a different question.

| Layer | Techniques and coverage |
| --- | --- |
| **Portable tests** | Grade math, preset round trips and corrupt-input rejection, realism settings, visual parameters, Twilight transitions, camera math and patch contracts, topology, smoothing, BMD rebuilds, and depth-of-field quality logic. No SDK or GPU required. |
| **Native build checks** | Dawn WGSL parsing/type checking on its Null backend and package manifest, library layout, and binary architecture checks. Shader validation can skip when the required adapter is absent. |
| **Visual comparisons** | Fixed save, camera, resolution, and time of day; baseline versus enabled captures; passthrough and neutral identity; A/B and difference views; clipping, focus orientation, silhouettes, and HUD exclusion. |
| **Lifecycle regression** | Enable/disable, movement, combat and special camera transitions, stage changes, resize, resource unload/reload, mod reload, and graceful shutdown with restoration checks. |
| **Performance** | Reset CPU timing samples between runs; compare distributions and pipeline/snapshot counts. Use host GPU captures or a platform profiler separately for GPU cost and total frame timing. |

Run the portable suite locally:

```sh
cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```

The [CI workflow](.github/workflows/build.yml) builds Windows AMD64 and Intel macOS packages and runs the native suites on pushes to `main`, pull requests, and manual runs. Version tags publish platform assets after those jobs succeed. Test presets fail when no tests are registered.

For reproducible runtime reports, record the MidnaFX version, exact Dusklight revision and applied patches, OS/GPU/backend, resolution, MSAA and bloom settings, scene/save, other mods or texture packs, and enabled features. Follow the [runtime checklist](docs/runtime-validation.md) and [performance procedure](docs/performance.md); keep build results, visual evidence, CPU measurements, and GPU measurements distinct.

## Repository guide

| Path | Contents |
| --- | --- |
| [`src/`](src/) | Native mod entry point, configuration and presets, game integration, renderer, and settings UI. |
| [`shaders/`](shaders/) | WGSL grading, passthrough, depth reconstruction, and depth-of-field shaders. |
| [`tests/`](tests/) | Deterministic C++ tests, shader validation, and Python contract checks. |
| [`tools/`](tools/) | Build, install, and host-patch helpers. |
| [`patches/`](patches/) | Source-matched Dusklight extension patches; separate from the mod package. |
| [`docs/`](docs/) | Architecture, research, feature investigations, validation evidence, and release notes. |
| [`mod.json`](mod.json) | Mod identity and release version. |

Useful deep dives: [source research](docs/research.md) · [detail processing](docs/detail.md) · [Twilight integration](docs/m5-m6-investigation.md) · [geometry coverage](docs/m8-geometry-coverage.md) · [depth foundation](docs/atmosphere-investigation.md) · [changelog](CHANGELOG.md).

## Issues and contributions

Report bugs or propose focused improvements through [GitHub Issues](https://github.com/rcreecy/midnafx/issues) and pull requests. Include reproduction steps, expected and actual behavior, the runtime details above, and relevant logs or matched captures. For visual changes, identify the exact settings and scene used. Do not attach game images or copyrighted game assets.

For code contributions, run the relevant portable tests and native checks when available, explain behavior changes, and state which runtime checks were actually performed. Geometry allowlist additions need fingerprint, topology, visual, restoration, and performance evidence. Host integration changes should preserve fallback behavior when services or supported resources are unavailable.

## License and credits

MidnaFX is [MIT licensed](LICENSE). It builds on the [Dusklight project](https://github.com/TwilitRealm/dusklight) and its Aurora/WebGPU rendering foundation. Upstream projects and dependencies retain their own licenses. The Legend of Zelda: Twilight Princess belongs to Nintendo; MidnaFX is an independent community mod and is not affiliated with or endorsed by Nintendo.
