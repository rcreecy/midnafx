# Native material relighting

Material relighting evaluates each supported native material's RGB lighting at
the fragment instead of interpolating lighting calculated at vertices. It uses
the material's real normal, ambient and material color sources, active light mask,
and native diffuse, spot and specular attenuation. The original texture and TEV
combiner stages still determine the final surface color. No asset list or texture
filename classification is involved.

Enable **Basic / Material relighting (experimental)** or select **Remaster** on a
compatible host. Relighting defaults on when its setting is unset. Vanilla disables it.
Existing saved scene looks without this feature bit load with relighting off.
Stock hosts keep native rendering and report that the control is unavailable.

## Host extension

The mod accepts optional `dev.twilitrealm.dusklight.native-material` from 1.0.
Version 1.1 adds an append-only `set_surface_relief` callback. Version 1.2 appends
`set_surface_profile` for per-draw finish parameters. The importer keeps
its minimum at 1.0 and checks the returned version, structure size, and callback
before requesting relief. Older extended hosts keep ordinary relighting; stock
hosts keep their original rendering. The
stock SDK can build the mod; stock Dusklight does not expose the service. Build
the pinned host and its Aurora submodule after applying the two source patches:

```powershell
python tools/material-relighting-host-patch.py --dusklight-dir path/to/dusklight
python tools/material-relighting-host-patch.py --dusklight-dir path/to/dusklight --apply
```

The first command checks applicability without editing files. `--reverse` removes
both patches from the same pinned source checkout. Build Dusklight using its
native build procedure and keep the executable name `dusklight.exe`. Use a
separate host directory and install the MidnaFX mod package normally.

The service tracks one game-thread request owner. Scene boundaries record one
copied mode byte in FIFO order; the render worker receives no mod pointers or
callbacks. The host ends the scope before HUD rendering. Disable and detach
release ownership. Scope changes invalidate the cached pipeline; the mode is
part of the shader key. Projection or offscreen eligibility changes also prevent
reuse of an incompatible pipeline. This path adds no stage callback drains,
offscreen targets or draws. Service 1.2 adds one shared sixteen-byte uniform
field to native lit shaders and their enhanced variants. Existing uniforms are rebuilt when
the native pipeline cache invalidates them.

Both variants share the same uniforms and bindings. While an enhanced pipeline
compiles asynchronously, the encoder draws with the ready native pipeline. This
avoids holes during first enable without waiting for shader compilation. The
host draw record reuses its redundant vertex range slot for the fallback
reference and retains the existing inline payload size.

## Coverage and preservation

Supported draws are depth-tested, color-writing, main-target perspective
triangles with a native normal attribute and a TEV-sampled lit RGB channel.
Native strips/fans/quads use their ordinary triangle conversion. Register and
vertex color sources, both native RGB channels, transformed/skinned normals,
native light masks and animated texture/color state use the same shared path.
Offscreen buffers, orthographic draws, lines, points, unlit materials and draws
without normals keep the native shader.

The shader normalizes the interpolated view-space normal. An unusable normal or
position retains the interpolated native RGB result. Invalid light distance,
coefficients or attenuation are guarded in the enhanced path. It preserves the
original vertex alpha path, including when a TEV raster swap uses native RGB as
an alpha input. The existing alpha test, blend order, fog, depth, texture
bindings, replacement selection and matrices remain in the native pipeline.

The base relighting covers the native light model. Baked lighting and unlit surfaces do not
provide a recoverable dynamic lighting model. Physical roughness, metallic and
normal-map inputs need authored data and a compatible material shader contract.
No PBR maps, extra light transport or new shadows are inferred. The catalog's
original asset identities remain available for future authored profiles.

## Distinct material finishes

The November 11 service 1.2 build varies the shared response by recognized native
material and primary texture names. English and established Japanese surface
terms identify broad classes rather than individual archive/file allowlists.
Conflicting class evidence and unrecognized names retain the existing shared
response; explicit wet labels select a wet coating. Apparel/hardware terms are
heuristics, not a guarantee of physical composition or artist intent.

| Finish | Highlight exponent | Highlight base | Relief multiplier |
| --- | ---: | ---: | ---: |
| Existing fallback | 14 | 0.12 | 1.00 |
| Rock | 8 | 0.055 | 1.30 |
| Wood/bark | 18 | 0.085 | 0.85 |
| Cloth | 6 | 0.025 | 0.12 |
| Metal | 96 | 0.32 | 0.18 |
| Wet | 64 | 0.25 | 0.40 |
| Leather | 28 | 0.13 | 0.30 |

Metal highlights take a restrained tint from the original UV sample; the wet
coating slightly darkens the base response. Rock keeps broader restrained
highlights and more relief. Cloth and leather use different highlight widths.
These are bounded art-direction parameters, not measured PBR roughness values.
Relief remains restricted to supported scenery; character surfaces keep their
native geometry normals and receive the supported highlight response.

Classifications cache on resource load independently of diagnostics. A bounded
resident sweep restores cache coverage after module reload, with at most four
models processed per update. The registry caps at 65,536 material entries;
shared ambiguous ownership and missing/copied bindings fall back. Archive
deletion invalidates its entries. Runtime lookup does not rescan texture pixels
or material names. Original names remain authoritative when the user installs
replacement images, while detail comes from the image actually sampled by the
native pipeline.

Each shape callback records its finish into FIFO order and restores the previous
finish afterward. The host accepts requests only from the relighting owner.
Scene boundaries, disable and detach restore the fallback. The worker receives
only copied numeric data. All native lit shaders and enhanced variants share an
extra sixteen-byte response field, preserving native fallback layout during
asynchronous compilation. Shader pipeline-cache schema 14 retires older layouts.
Changing finishes updates uniforms instead of creating shader permutations.

The existing Material relighting toggle enables finishes on service 1.2 hosts.
Older hosts keep their previous behavior. No new texture reads, texture maps,
geometry or render passes are added. This first pass caches semantic profiles;
it does not generate separate raster normal/roughness maps, infer true material
properties from albedo, or add scenery reflections to opaque objects. Existing
UV-derived relief provides the surface-detail approximation.

Ordon and Forest Temple original-texture previews and a Faron Spring 4K-pack
preview render and close normally. Faron requests rock, wood, cloth, leather and
metal finishes. Wet labels pass classifier checks but have no new native visual
receipt. Forest Temple resumes wood/metal requests after live module reload.
Four focused checks and both host patch applicability directions pass. See
[the finish receipt](evidence/material-finishes-2026-11-11/README.md) for hashes,
pack DDS fallbacks, reload observations and limits. Whole-game artistic coverage,
GPU duration and macOS gameplay remain unverified.

## Soft surface highlights

The November 11 host build adds a broad, satin-like light response to supported
opaque scenery and characters. It evaluates a view-dependent half-vector lobe
against the material's enabled native lights, including their color and bounded
spot/distance attenuation. Broad highlights and a restrained grazing response
give surfaces more rounded visual volume. This interprets the requested art
direction; it does not describe the reference game's rendering implementation.

The highlight follows normalized interpolated normals with a smaller contribution
from texture relief. The native diffuse and texture colors remain present. The
added RGB is capped at 0.22 per channel and applied after TEV, before native fog.
Native alpha remains unchanged. Multiple sampled lighting channels combine by
maximum rather than adding duplicate highlights. Channels using native specular
attenuation retain their existing response without an additional highlight.

This path requires an opaque, depth-writing, lit native draw with no alpha test.
Blended effects, cutout foliage, unlit surfaces, offscreen targets and HUD draws
retain their existing response. The shared path has no asset or stage allowlist.
It adds arithmetic to the existing pass, without new texture reads or targets.
It does not add subdivisions, smooth hard mesh boundaries, or infer authored
roughness/metallic maps. Different material finishes still need richer inputs.

The existing Material relighting toggle also controls this response on the new
host build. The service contract remains 1.1; older hosts retain their previous
lighting behavior. The mod package is unchanged from the pollen/firefly build.
Only updating the mod cannot add these host shader changes.

Ordon and Forest Temple Windows D3D12 previews activate the new shader, reach an
enhanced ready pipeline, and close normally without logged shader validation
errors. The source patch passes forward and reverse applicability checks.
Compatibility, preset and package checks pass. See the
[November 11 receipt](evidence/material-sheen-2026-11-11/README.md).
Whole-game artistic coverage, GPU duration and macOS gameplay remain unverified.

## Texture relief on solid scenery

The November 10 host extension adds bounded texture-derived relief to supported
lit scenery. It uses luminance from an existing native TEV texture sample as an
approximate height signal, computes a surface gradient from view-space geometry
derivatives, and perturbs the normal used by native fragment lighting. The
original image, UV transforms, replacement selection, TEV colors, alpha, depth,
and silhouette remain intact. This is a lighting approximation, not an authored
height/normal map, displaced geometry, or a complete physical material model.

Relief is scoped to the early native background/terrain pass. Regular character
draws occur afterward and retain ordinary per-fragment lighting. Eligibility
also requires a depth-writing opaque blend state, no alpha test, a normal
attribute, a sampled lit RGB channel, and an ordinary TEX0-driven 2D texture
coordinate. Indirect and environment-mapped texture paths bypass relief. The
first qualified original UV sample supplies height; no asset allowlist is added.
The same shader path can consume a user-installed replacement texture.

Height response uses a six-world-unit scale with normal-gradient magnitude
limited to 0.42. It fades between 1,400 and 4,200 units to reduce distant facet
contrast. Invalid/degenerate gradients retain the original normal. Native alpha
and transparency paths are unchanged. The implementation adds no texture reads,
uniform fields, render targets, or draw passes, but adds fragment arithmetic on
qualified draws. GPU duration and 4K cost remain unmeasured.

The existing **Material relighting (experimental)** toggle requests relief on
service 1.1 hosts. The host requires the same request owner for both callbacks.
Disable and detach clear both requests. One copied FIFO mode byte selects native
rendering, ordinary relighting, or scenery relief at scene boundaries; the
relief bit ends after terrain, before regular characters. The mode remains part
of the pipeline key, including the existing native fallback during compilation.

Windows D3D12 previews exercise the extended shader in Ordon and Forest Temple
room 3, with original textures. Ordon's comparison shows added surface variation
on stone/scenery while Link remains visible. The Temple preview retains its
wooden platform and native scene. Both log the original-UV relief shader and
close with exit code zero, without logged WGSL/GPU validation errors. An Ordon
run on the older 1.0 host also closes normally with ordinary relighting. These
unfrozen views do not prove exact pixel parity, every material class, motion
stability, or whole-game visual quality. Replacement-pack relief, native toggle/
reload checks for this extension, and macOS gameplay remain unverified.

The compatibility test exercises service 1.0, appended/truncated 1.1 layouts,
missing callbacks, ownership-conflict retry, repeated update, disable, and
shutdown. That test, preset round-trip, and package-contract checks pass. Both
updated patches pass forward checks against pinned Git indices and reverse
checks against applied files.

Local evidence is in `build/material-relighting/relief-before-ordon/`,
`relief-after-ordon/`, and `relief-temple/`. The updated host is installed in
`build/material-relighting/host-final/`; that directory now receives subsequent
host updates. The pre-relief host remains in `host-before-relief/`.
The package in the November 10 relief trial has SHA-256
`EB58D10FF694E444F9754322690206ED8450AB7FCE11CC265424879F73A5343D`;
the relief host in that trial has SHA-256
`6AFEF114F9F012E397793D4A18A325E28EF2FB4BC8A92AC791E8AA04FAB601B7`.

The enhanced shader keeps the vertex RGB fallback and adds fragment work for
lit channels. No GPU duration or 4K performance claim is made. macOS support is
best effort; only the working Windows host can exercise native rendering here.
Hosted Metal execution is excluded.

The [Windows evidence](evidence/material-relighting-2026-11-03/README.md) records
stock-host fallback, original and replacement texture trials, native warmup
fallback, enhanced draw encoding, toggle, live reload and normal close.
