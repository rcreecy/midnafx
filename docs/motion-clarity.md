# Motion clarity

MidnaFX 1.12 adds **Motion clarity** across gameplay contexts, without asset or
stage lists. It reduces the native previous-frame blending that can leave ghost
images during movement. Select **Profile / Remaster** or enable the independent
Basic toggle. **Vanilla** disables it. Existing saved settings remain intact
until a profile is selected.

**Motion blur retained (%)** in Advanced: motion defaults to 0%, removing this
particular blend during eligible gameplay. Values through 100% scale the native
blend alpha, with rounded integer output. At 100% the native value is unchanged.
This controls the game's frame blending, not a physical shutter angle or
motion-vector reconstruction. Combat feedback using the same native frame blend
is also reduced during eligible gameplay. Turn the feature off to preserve it.

## Scope and ownership

The feature bypasses missing-player, paused, active native event, demo-camera and
fade contexts. Authored event/demo blur retains its native rate. It does not
change native bloom, depth blur, camera shake, hit stops, camera motion, character
animation, input or gameplay speed.

The mod borrows only the native blur-rate byte after opaque rendering. Native
motion-blur recording consumes that reduced alpha later in the same frame. A
pre-HUD hook restores the original rate. The native enable flag, history latch,
blur matrix and framebuffer copies remain under game ownership. The next scene
begin and module shutdown also recover a pending borrowed value if a prior
frame-end callback was skipped.

Restoration writes only while the current byte still equals the mod's applied
value. A detectable intervening change from the game or another mod is preserved.
Equal-value writes cannot be distinguished through this value-based ownership
check. Disabling restores the native request for subsequent draws; it cannot
reconstruct blur history from frames already presented.

No shader pass, render target, GPU readback or new host patch is added. The
existing native blend draw may still execute with zero alpha, so no GPU speedup
is promised. Unsupported stage-hook registration bypasses the feature. macOS
support remains best effort through native builds and portable checks; native
gameplay is unverified.

See [the evidence ledger](evidence/motion-clarity-2026-10-28/README.md) for rate,
ownership, safety-gate and native runtime results. Whole-game motion comfort and
authored-effect coverage remain open.
