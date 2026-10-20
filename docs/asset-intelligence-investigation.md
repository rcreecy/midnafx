# Asset intelligence investigation

Follow-up: the [filtered native draw experiment](asset-draw-correlation.md) now proves pumpkin material-to-WebGPU encoding on Windows using an explicit experimental host patch. Replacement selection and GPU completion remain unobserved.

Date: 2026-10-19. Scope: an opt-in, read-only identity proof, not a shading implementation or release. The existing package version remains 1.2.0. The working baseline was `a72340e` on `codex/m12-water`.

## Evidence and compatibility boundary

The audit uses the checked-out primary sources: Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b` and Aurora `7f2801cd0133c9333eadb4e2e6b24100c328d328`. SDK services are public contracts; native J3D classes and hooked functions are revision-specific implementation details. The local runtime retains the existing camera, menu, and clock test patches. This work adds no host patch. Those patches are not necessary to compute asset identity; the console/scene controls made the experiment reproducible.

Existing evidence includes [geometry coverage](m8-geometry-coverage.md), the [geometry observer](../src/game/geometry_probe.cpp), the [BMD reader](../tools/inspect-bmd.py), and the existing topology/smoothing tests. The implementation reuses their resource hooks and archive reader rather than adding a second hook framework or resource loader. Build and CI definitions remain [CMake](../CMakeLists.txt), [tests](../tests/CMakeLists.txt), and [the native workflow](../.github/workflows/build.yml).

## Capability matrix

| Relationship or control | Existing evidence | Reuse / extension | Conclusion |
|---|---|---|---|
| Archive to original model bytes | `dRes_info_c::loadResource` and `JKRArchive::getIdxResource`; existing geometry fingerprints | Add pre-load observation to the existing hook | Proven for the pumpkin. Matching offline SHA-256 strengthens the runtime FNV join. |
| Loaded model and archive ownership | `mRes[fileIndex]` after loading; `deleteArchiveRes` before freeing | Reuse post-load and delete hooks | Proven through a scene round trip. |
| Material index and texture references | MAT3 remap/init/texture-number tables; loaded `J3DMaterial` TEV blocks | New bounded MAT3/TEX1 parser and live initial-slot observer | Both pumpkin materials match. Runtime slot capacity must follow the concrete TEV subclass. |
| Original texture identity | Aurora `hash_texture_source`; TextureService source key | Compute the same XXH64 base-mip key offline | Four pumpkin entries contain two shared keys. This is not an object identity. |
| Multiple model instances | `mDoExt_J3DModel__create` post-hook | Record successful creations against shared model data | Twelve creations per Ordon load. Actor identity, lifetime and visibility are not established. |
| Replacement registration | TextureService v1 data/file registration and unregister | No registration needed for read-only discovery | Public API can install replacements, but cannot enumerate the selected replacement. |
| Replacement candidate matching | Aurora DDS/PNG source-key filename parser | Offline exact-key candidate inventory | Synthetic fixture and actual Henriko pumpkin filenames match. Active per-draw selection is unobserved. |
| Native material to GPU draw | GfxService stage hooks and custom draw callbacks | Would need a narrow host metadata observer | Public service lacks native J3D model/material identity on submissions. Stretch objective blocked. |
| Existing selective geometry/water work | Geometry replacement and water probe code | Future reuse only | Demonstrates narrow native integrations; does not establish a generic asset-aware shading API. |

## Primary source map

These paths refer to the pinned, ignored upstream checkout. They are intentionally distinguished from stable SDK contracts:

- [Resource loading](../upstream/dusklight/src/d/d_resorce.cpp): `loadResource` around line 327, model assignment to `mRes` around line 536, and `deleteArchiveRes` around line 546.
- [TextureService v1](../upstream/dusklight/sdk/include/mods/svc/texture.h): source and pointer keys, copied data, registration ownership and priority. Later mod registrations override earlier mods, which override the user texture directory. No selection query or enumeration function is present.
- [GfxService v2](../upstream/dusklight/sdk/include/mods/svc/gfx.h): `GfxDrawContext`, `GfxStageContext`, custom draw registration, resolved targets and render-worker ownership. A custom draw callback is not an observer of every native draw.
- [Aurora source hashing](../upstream/dusklight/extern/aurora/lib/gx/texture.cpp): `hash_texture_source`, around line 281. The replacement key uses XXH64(seed 0) over the padded base mip. Palette formats also hash the referenced TLUT range. The internal render-cache hash uses XXH3-128 over different data; it is not interchangeable.
- [Replacement filename parsing](../upstream/dusklight/extern/aurora/lib/gfx/texture_replacement.cpp): `tex1_` parsing around line 1391, DDS/PNG handling, wildcard/mipmap selection and streaming. [Dusklight directory setup](../upstream/dusklight/src/dusk/texture_replacements.cpp) uses `ConfigPath/texture_replacements` when `game.enableTextureReplacements` is enabled.
- [J3D loader layout](../upstream/dusklight/libs/JSystem/include/JSystem/J3DGraphLoader/J3DModelLoader.h), [material factory](../upstream/dusklight/libs/JSystem/include/JSystem/J3DGraphLoader/J3DMaterialFactory.h), [material blocks](../upstream/dusklight/libs/JSystem/include/JSystem/J3DGraphBase/J3DMatBlock.h), and [texture headers](../upstream/dusklight/libs/JSystem/include/JSystem/JUtility/JUTTexture.h) define the layouts used here.

## Replacement availability and limits

The Henriko `ZTP 4K 4.0d (4K, PC Edition).zip` became available in Downloads during validation. It was extracted to an isolated ignored test directory, then connected to the isolated host's `texture_replacements` directory. The host reported 1,759 registrations. Both pumpkin source hashes matched actual DDS files; their headers describe 1024×1024 and 2048×2048 images, despite the pack's “4K” name. Original filename dimensions remain 64×64, as required for source lookup. See the [pack manifest](evidence/asset-intelligence-2026-10-19/pack-manifest.json) for file and archive hashes. No texture payload is committed.

The original/replacement separation follows the pinned renderer's lookup design and passed the live pumpkin comparison with this pack enabled. The exact replacement selected for a particular GPU draw remains unobserved. Archive overlays, another mod changing model bytes, animated texture bindings, or runtime-generated images can invalidate the broader preservation assumption. [Validation](asset-intelligence-validation.md) lists actual observations and [architecture](asset-intelligence-architecture.md) defines the limits.
