# Twilight intelligence and upstream-rendering investigation

Source audit against pinned Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
M5 also has a focused Windows runtime result. M6 remains an architecture decision.

## M5: semantic state and automatic profile

`d_s_play.cpp` sets `world_dark` to 1 for an active darkworld stage, 2 for a
darkworld spot, and 0 otherwise. `dKy_darkworld_check()` tests equality to 1.
MidnaFX reads `dComIfGp_world_dark_get()` during `mod_update` on the game thread
and selects only 1. The player pointer is the scene-ready gate:
`dComIfGp_getStage()` always returns an address of embedded storage, whereas Link's
actor registers the player pointer at `d_a_alink.cpp:4937` and clears it at
`:19903`; the name scene also clears it at `d_s_name.cpp:133`. This gate may
temporarily exclude unusual playerless gameplay, which should be checked at runtime.
No game pointer is held or read by the render worker.

The automatic profile is opt-in and inert until the user captures a Twilight
target from the manual controls. A captured target is a separate MFX2 snapshot;
it does not replace the general look or its Custom storage. Transition duration
is configurable from 0 to 3 seconds. Each game update moves a bounded blend
weight toward 1 for active Twilight and 0 for normal, spot, or unavailable
state. The game-thread stage interpolates prepared uniforms; no shader variant,
extra snapshot, or pipeline build is added. Diagnostics display sampled state,
target availability, and blend weight, and optionally log state and endpoint
changes. There is no built-in claim that any chosen Twilight look is visually better.

On 2026-09-25, the source-matched Windows D3D11 host loaded `F_SP108` as Normal
and held the automatic blend at 0%. It loaded `D_MN08` room 0 as Active Twilight,
then logged a transition from 3% to 100%. Matched captures with the same direct-stage
checkpoint showed the deliberately extreme test target was applied only when the
automatic profile was enabled. This proves the runtime state feed, target decode,
transition, and render application for normal and active states. State 2 (Twilight
spot) remains covered by deterministic tests but has no live capture. Entry/exit,
pause, unusual playerless scenes, and mod reload remain broader compatibility checks.

The mod now declares the SDK `game` feature alongside `webgpu`, so the loader's
game ABI epoch check applies. It is pinned to the researched Dusklight revision.
This raises binary compatibility requirements compared with the generic M4 mod;
CI linkage alone cannot prove compatibility with a different game build.

## M6: available intervention points

Dusklight's environment pipeline is distributed. `d_kankyo.cpp:2283` sets
environment light and interpolates palette, fog, and bloom parameters;
`d_kankyo.cpp:4457` assigns material fog ranges; `:9396` and `:9431` issue
`GXSetFog`. `m_Do_graphic.cpp:1699` draws native bloom. Weather particles and
mist are drawn as scene content. The existing MidnaFX pre-HUD stage occurs at
`m_Do_graphic.cpp:2759`, after bloom and some 2D/game particle work but before
ordinary HUD. A final color pass cannot separate these contributors once
composited.

The public GfxService exposes stage hooks, color snapshots, and optional raw
depth snapshots. Its `resolve_pass` may report depth unavailable on a device
(`aurora/lib/gfx/recording.cpp:961–973`). A raw depth snapshot at the pre-HUD
stage does not reconstruct already-applied material fog, translucent mist,
sky, or particle ownership. A depth-based fog replacement would need verified
projection, depth convention, inclusion ordering, and a semantic world mask.
Adding it now would be an ungrounded fullscreen effect with an extra 4K depth
snapshot and unknown Intel GPU cost.

The developer bloom override at `src/dusk/imgui/ImGuiBloomWindow.cpp:57` proves
native parameters are adjustable, but it is global developer state, not a
mod-scoped reversible environment service. Direct writes to environment
globals or hooks in `setLight` would compete with the game's interpolations
and possibly the host's bloom controls. No stable per-scene ownership and
restore contract is exposed by the pinned SDK. M6 therefore stops at this
source-level design gate: there is no upstream fog, bloom, lighting, overlay,
particle, or geometry modification in this checkpoint.

Next evidence needed for an M6 implementation: controlled same-scene captures
with native bloom modes, M4 grading, and the M5 target on/off; a Metal capture
for pass ordering and depth availability; and a documented ownership/restore
interface for any native parameter that MidnaFX would change. A dedicated
Dusklight service or upstream patch may be cleaner than a mod-side direct
write. See `docs/notes/game.md` for the broader source map.
