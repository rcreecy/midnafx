# Selective material control proof

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

A complete successful archive departure/return, broader motion/scene coverage,
active sampling/replacement-reload timing and actual allocation cost remain open.
Eight [settled timing trials](frame-timing.md#settled-comparison-2026-10-22) now
cover baseline, diagnostics off/on, and body control separately. macOS compilation remains a CI
check; native macOS gameplay/Metal verification is unavailable and best effort.
This is a verified static material-control slice within the ongoing remaster
program, not closure of the complete R1 lifecycle or the full product goal.
