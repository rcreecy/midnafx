# Selective material control proof

Qualification: 2026-10-21 through 2026-10-22, Windows/D3D12, 960 × 720.
This opt-in developer proof dims the body of the recognized original pumpkin
model. It establishes a visible, reversible material operation independent of
texture-provider selection. It is not a remaster look, normal mapping, PBR,
generic material control or per-actor-instance isolation.

Enable **Material proof: dim pumpkin bodies** before loading the scene.
The saved setting `pumpkin_body_control` defaults to false. A newly enabled
binding requires an archive/scene reload; once bound, off/on changes are live.
Every instance sharing that bound body material changes. Leaves retain their
separate material. Discovery and draw diagnostics are not required.

## Ownership and operation

The game-thread archive hook recognizes only
`pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421`. The loaded model must have
two materials. Material 1 must retain the qualified single-stage RGB operation
(`0xc0`, operation `0x28`, inputs `0xf8/0xaf`). Qualification repeats before
each affected draw. Unexpected formats or stages retain their original draw.
Archive pre-delete and mod shutdown clear the session-only binding.

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

Archive departure/return, enabling before a later load, mod unload/reload while
enabled, broader motion/scene coverage and measured frame/allocation cost are
not yet qualified for this new write path. macOS compilation remains a CI
check; native macOS gameplay/Metal verification is unavailable and best effort.
This is a verified static material-control slice within the ongoing remaster
program, not closure of the complete R1 lifecycle or the full product goal.
