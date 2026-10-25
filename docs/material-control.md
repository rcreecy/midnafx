# Selective material control

## Authored warm profile: 2026-10-24–25

**Warm pumpkin material (experimental)** adds a subtle authored RGB gain to the
recognized original pumpkin body. The saved setting `pumpkin_warm_profile`
defaults to false and works independently of the dim proof and diagnostics.
It supports original textures and external replacements, including an already
loaded original archive. Every instance sharing the qualified body material is
affected; leaves retain their separate material.

The profile writes XF material color zero (`0x100c`) as RGBA `(255,240,224,255)`
for the outer body draw and restores its qualified white `(255,255,255,255)`
afterward. The original TEV stage, one color channel, register-sourced RGB/alpha,
`GX_COLOR0A0` order and white material color must still qualify before each
application. It reuses the exact resource/source and archive-lifetime checks
described below. A captured per-draw flag controls restoration even if the UI
request changes between callbacks. This is a known-white static contract, not a
backup of arbitrary live GX state or ownership proof against another mod.

The pinned Aurora path multiplies this material color by the existing ambient
and light accumulation. The profile leaves lighting controls, alpha, texture
bindings, TEV operations and shader configuration unchanged. `drawFast` and the
original pumpkin shape lists do not overwrite that XF material register.
Portable command tests independently decode the XF transfer, check the address,
padding, alpha and register isolation, and prove white restoration. The pot
material uses vertex-sourced color and animated TEV color inputs; it does not
qualify for this interface and remains unchanged.

The [profile ledger](evidence/material-profile-2026-10-24/README.md) separates the
initial pixel candidate from the final package's logging-only correction. The
candidate's original off/on/off and replacement on/off/on comparisons reproduce
the sampled body exactly on the third image; five sampled exclusion rectangles
remain identical throughout. These are rectangle comparisons, not whole-frame
or broad motion approval. The final package passes native mod reload and cached
binding; a bounded REA probe observes 114 warm/restore pairs with no other matching
color or read faults after reload. Its actually loaded module hash matches the
[artifact baseline](evidence/rea-artifacts-2026-10-24/README.md).

On 2026-10-25 the final package also completes an Ordon–Fishing Pond–Ordon archive
round trip with replacements enabled and diagnostics disabled. Deletion clears
the binding at 15,216 balanced pairs, the return binds a fresh material at the
same count, and normal shutdown reaches 21,480 balanced pairs with zero
rejections. A process handle acquired before closing retains exit code zero.
Lifecycle views include different native scene lighting and are not matched
pixel comparisons. REA instrumentation is not a performance baseline or proof
of GPU completion.

This is the first authored material profile, not completion of R2's several-class
target. Additional material classes, representative motion/lighting coverage and
actual GPU/allocation cost remain open. macOS compilation and packaging remain
supported; native macOS rendering is unverified and best effort.

## Next material candidate

The next read-only lead is `L_mbox_00.arc/l_metabox_00.bmd`, material 0
`L_metabox00`: 35,040 bytes, original FNV `640f192e3c9a1623`, SHA256
`52c7020e943d18e2e5b278ef005a44e7fb51295fc9ef68fde86b6fcd8ad91974`.
It has one joint/material/shape, register-sourced RGB and alpha, white material
color and `GX_COLOR0A0`. Its indexed shape list has six triangle strips and no
XF/BP writes. The pinned metal-box actor sets environment lighting and draws;
no material-color animation is identified in that path.

Its RGB stage is `28/e8/a2`, not the pumpkin's `28/f8/af`. The stage approximately
combines `4*((1-RASC)*K0 + RASC*TEXC + C0)` before clamping. Changing XF material
gain therefore changes an interpolation weight, not uniform albedo gain.
Keep dynamic K0/C0 and light inputs intact; do not describe this as metallic or
specular shading. A future profile needs its own exact identity/stage qualifier,
native visual comparison and lifecycle checks. Pumpkin-specific cached source
normalization must not be reused without independent qualification. Native load
binding is the narrower initial option.

Existing [geometry research](geometry-investigation.md) records a target load at
`D_MN04,7,0,-1` and actor draws at `F_SP116,3,13,2`; natural unobstructed framing
remains unqualified. Earlier visible geometry proof used temporary render
relocation. The wood-box alternative has similar stage inputs but its burn-box
actor mutates C0, so it requires a separate draw contract. None of these leads
extends the implemented warm profile's allowlist.

## RGB-scale proof

Qualification: 2026-10-21 through 2026-10-22, Windows/D3D12, 960 × 720.
This opt-in developer proof dims the body of the recognized original pumpkin
model. It establishes a visible, reversible material operation independent of
texture-provider selection. It is not a remaster look, normal mapping, PBR,
generic material control or per-actor-instance isolation.

Enable **Material proof: dim pumpkin bodies** before or after loading the original model.
The saved setting `pumpkin_body_control` defaults to false. Already loaded original
memory archives can bind on the next enabled outer shape callback. Off/on changes are live.
Every instance sharing that bound body material changes. Leaves retain their
separate material. Discovery and draw diagnostics are not required.

## Ownership and operation

The game-thread archive hook recognizes only
`pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421`. The loaded model must have
two materials. Material 1 must retain the qualified single-stage RGB operation
(`0xc0`, operation `0x28`, inputs `0xf8/0xaf`). Qualification repeats before
each affected draw. Unexpected formats or stages retain their original draw.
Archive pre-delete and mod shutdown clear the session-only binding.

Shape hooks install only after resource-load, model-load and archive-delete hooks
are ready. Cached binding performs one game-thread lookup per observed enable
transition, with no resource fetch, disk access, reference increment or repeated
draw-loop scan. It rejects in-progress loads, non-memory archives, invalid bounds,
wrong node/name/source identity, overlays and rebuilt inputs. A packed archive
may have a null data heap; that is not a readiness failure.

The pinned little-endian native loader swaps vertex, joint and descriptor data
in place. Cached source qualification therefore uses normalized post-load FNV
`7073c7ae329d074d`, size 17,824, and exact equality between the model's raw source
pointer and resident archive source. Only the twelve compiler-padding bytes at
`0xcd–0xcf`, `0xdd–0xdf`, `0xed–0xef`, and `0xfd–0xff` are read as `0xff` during
hashing; live memory is never rewritten. All other bytes affect identity.
The original pre-load fingerprint remains unchanged.

`tools/qualification/material-source-normalization.py` independently simulates
the pinned native conversion from the locally supplied original archive. Its
normalized result matches all 17,824 bytes of a noninvasively captured live source,
with zero unexplained changes. Raw game bytes remain in ignored local output.
The portable test checks that each of the other 17,812 bytes affects the digest
and that pre-load hashing does not ignore padding.

The existing shape pre/post hook brackets the pinned non-recursive `drawFast`
path. A masked BP command reduces stage-zero RGB from SCALE_4 to SCALE_2;
the matching post callback restores SCALE_4. Alpha, texture bindings and other
RGB bits are not written. The native FIFO copies both aligned command buffers,
so rendering retains no pointer into game material memory. The UI publishes
only an atomic request; it does not dereference game resources. Pending restore
does not depend on the current request. Bounded callback scopes match the
native invocation's argument-array identity, including skipped-pre callbacks.

Restoration uses this static material's validated definition. It is not a
snapshot of arbitrary live GX state. Animated stage changes, other mods that
override the same live scale, arbitrary recursive shape rendering, other
resources and generic material ownership require a stronger native control
contract. Do not generalize this seam without that work.

## Retained observations

Evidence: [material-control-2026-10-22](evidence/material-control-2026-10-22).
Each trial retains three unmodified screenshots (off/on/off), screenshot hashes,
bounded material-control logs, relevant asset events, host/package identity,
settings and measured pixel rectangles. Simulation was paused with `rate 0`
while presentation continued. These are static comparisons, not a motion or
performance qualification. A missing physical controller produces the visible
native notification; its region is excluded from pixel comparisons.

| Check | Original textures | External replacements |
|---|---|---|
| Body visibly dims | Observed | Observed |
| Left-body rectangle mean absolute RGB change, 0–255 scale | 30.324 | 32.988 |
| Center-body rectangle mean absolute RGB change | 23.434 | 24.611 |
| Sampled leaves/ground/architecture/player off-versus-on difference | Exactly zero | Exactly zero |
| Every sampled rectangle off-versus-restored difference | Exactly zero | Exactly zero |
| Matched apply/restore count at shutdown | 7,555,884 / 7,555,884 | 47,400 / 47,400 |
| Rejected stage count | 0 | 0 |
| Normal close without forced termination | Observed | Observed |

The original trial has both diagnostic settings disabled throughout. The pack
qualification enables diagnostics to corroborate replacement selection:
32/32 labeled draws reach encoding, with the body using a 1024² replacement and
leaves using a 2048² replacement. This proves encoded binding; screenshots
provide the separate visible-effect observation. A preliminary pack smoke
trial with diagnostics disabled also binds, renders and closes normally; it
is not substituted for the retained matched pack comparison.

Pack source keys are `000e342106676e31` (body) and `630d5c7bc606585d` (leaves).
Payload SHA256 values are respectively
`ef46efe02e8ebd33992e407d782f0292466074d06d77cc08ac0109cb3cf5c9de`
and `b6ffce0e16b137ad73fa5ba3aea78d982f89dc1355e367ac40bc745e4439e86f`.
External texture files are not included in this repository or the mod package.

The command-buffer test independently decodes the BP writes, varies preserved
register bits, checks alpha is untouched, verifies padding/alignment, and tests
the qualified stage's rejection boundary. Windows package build and all 20
CTest entries pass, including the D3D12 pixel test. Code review identified and
repaired callback symmetry for nested calls whose pre hook was skipped.

## Remaining acceptance

The [lifecycle ledger](evidence/material-lifecycle-2026-10-22/README.md) retains
the failed Hyrule Field transition, successful Fishing Pond-to-Ordon load,
cached scene reload, reproduced mod-reload gap, repaired reload and normal closes.
The final normalized package rebinds after a native mod reload on the host without
draw tracing, with both diagnostic settings disabled and replacements enabled.
Apply/restore counts balance before and after reload (11,394 and 5,802 respectively).
The earlier cached-binding candidate's paused off/on/off comparison restores
every sampled rectangle exactly and leaves sampled unrelated regions unchanged.
Its package identity is kept separate from the final normalized package.

The final normalized package also completes a separate archive round trip through
Fishing Pond: `F_SP103,0,27,0` to `F_SP127,0,0,0` and back to Ordon. Archive
deletion clears the binding with 17,982 balanced applies/restores; returning binds
the original target again at the same count. At shutdown the total is
515,310/515,310, with zero rejections and `all mods unloaded` logged. Diagnostics
remain off and the existing replacements remain enabled. The ledger retains
three unchanged scene screenshots and their hashes. These are lifecycle views,
not matched pixel comparisons or a performance capture. The process exit code
was not retained. This successful route does not resolve the black `F_SP121`
transition, which remains a failed trial with unknown cause.

Broader motion/scene coverage, active sampling/replacement-reload timing and
actual allocation cost remain open.
Eight [settled timing trials](frame-timing.md#settled-comparison-2026-10-22) now
cover baseline, diagnostics off/on, and body control separately. macOS compilation remains a CI
check; native macOS gameplay/Metal verification is unavailable and best effort.
This is a verified static material-control slice within the ongoing remaster
program. The known archive round trip and native mod reload are observed;
broader R1 compatibility/quality and the full product goal remain unqualified.
