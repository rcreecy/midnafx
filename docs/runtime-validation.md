# Intel Mac runtime validation session

This checklist is for one Intel x86_64 Mac session with the current Dusklight build.
Compilation and GitHub Actions packages do not establish runtime or visual behavior.
Keep a fixed save, camera, in-game time, render scale, and native bloom mode when
comparing frames. Create a directory named with the date, Dusklight commit, and
MidnaFX commit; keep screenshots, logs, and notes together.

Before launch, record: (1) Dusklight revision, (2) MidnaFX revision, (3) Dawnlight
revision or OFF, (4) texture-pack name/version or OFF, and (5) Mac model, Intel GPU,
macOS version, display resolution, and render scale. Save the active `midnafx.dusk`
artifact and its source Actions run URL. Use lossless PNG when available. macOS
`screencapture -x -t png <filename>.png` is a fallback; note that a desktop capture
may include display scaling or color management, so it is not raw framebuffer evidence.

## Ordered test pass

1. Launch without MidnaFX, load the fixed normal-world save, and capture `normal-A-disabled.png`.
2. Install MidnaFX, relaunch, confirm the mod appears/loads, and record its log lines.
3. Confirm diagnostics report a ready pre-HUD graphics pipeline and the correct render size.
4. Leave **Enable grading** off. Confirm no MidnaFX snapshot/draw counter increase and capture the same scene if practical.
5. Enable grading and select **Passthrough** Debug View (or Force passthrough comparison). Confirm one snapshot and one draw per active frame; capture `normal-B-passthrough.png`.
6. Select **Diagnostic / Shader Smoke Test** and **Final** Debug View. Confirm all controls change in the UI, the status reports Grade + detail, and the framebuffer change is unmistakable; capture `normal-C-smoke.png`.
7. Change each grading control individually, return it to the smoke value, and verify its enabled toggle. Include exposure, black point, contrast, gamma, saturation, rolloff, temperature, and tint.
8. Toggle sharpening off/on at 0%, 20%, 35%, and 50%. Check fine texture, silhouettes, particle edges, halos, ringing, and grain. Capture `normal-D-smoke-detail.png` with detail on and the matching detail-off image.
9. Select **A/B Split** at 50%, then 25% and 75%. Confirm left is the source and right is processed; capture `normal-E-split50.png`.
10. Select **Luminance** and capture `normal-F-luminance.png`. It represents the observed scene UNORM domain, not physical luminance.
11. Select **Highlight Clipping** and **Shadow Clipping**, then capture `normal-G-highlight.png` and `normal-G-shadow.png`. Inspect bright lamps/sky and dark corners.
12. Select **Difference** with smoke grading and capture `normal-H-difference-smoke.png`. Restore Vanilla with detail off while keeping Difference selected; capture `normal-H-difference-neutral.png` and check that output is near black.
13. In Final view, compare HUD, minimap, menus, dialogue, and inventory with MidnaFX off/on. Check that ordinary HUD is excluded; record any 2D effect that appears to precede the stage.
14. Observe an in-game scene transition and fade. Watch for stale snapshots, wrong dimensions, one-frame flashes, or pipeline errors.
15. Resize and change render scale through 1080p, 1440p if available, and 4K. Record displayed dimensions, snapshot count, and pipeline count. A dimension-only change should not rebuild a pair.
16. Reload the mod from the mod manager. Check settings persistence, selected preset, saved Custom look, debug selector, clean shutdown, and no GPU validation errors.
17. Repeat steps 1–16 with Dawnlight OFF, then ON. Record same-stage ordering differences rather than assuming a fixed order.
18. Repeat the smoke, detail, split, and HUD checks with high-resolution textures enabled.
19. Capture the same A–H set in one stable Twilight Realm scene if possible; at minimum take `twilight-A-disabled.png`, `twilight-C-smoke.png`, `twilight-D-detail.png`, `twilight-E-split50.png`, and `twilight-H-difference.png`.
20. Collect MidnaFX and Dusklight logs, warning/error messages, CPU diagnostics, and any reproducible visual defect with exact settings and scene location.
21. If available, take a Metal GPU capture at 1080p and 4K. Separate the scene-copy/pass transition, fullscreen draw, and whole-frame GPU time. Follow `docs/performance.md` for disabled and active CPU p50/p95 measurements; CPU service-call time is not GPU duration.
22. For the M5 prototype, capture a conservative manual look as the Twilight target, restore a different general look, then enable Automatic Twilight profile. Record state and blend percentage in a normal scene, active Twilight, a Twilight spot, entry/exit, load, pause, and mod reload. Confirm state 2 never selects the target. Capture `twilight-M5-general.png` and `twilight-M5-auto.png` from one stable Twilight scene. Check whether playerless scenes or Palace rooms should be handled separately before treating detection as validated.
23. With the CameraService 1.3 host patch applied, record camera diagnostics during outdoor and indoor traversal, targeting, combat, aiming, first person, conversation, cutscene, item-get, horseback, swimming, climbing, scene transition, detached/free camera, aspect/resolution change, disable/re-enable, and mod reload. Only active mode-0 native chase output may report active FOV and chase-latitude modifiers; every other camera algorithm, authored event, or detached context must report inactive and restore native framing immediately. Capture matched `camera-vanilla.png`, `camera-wide.png`, and `camera-low.png` from the same position/orientation. Windows D3D11 evidence exists for the 110% FOV plus 6-degree lower candidate, startup event handoff, forced ownership guards, in-process fallback, and two startup aspect ratios. The algorithm gate is source- and contract-tested. Collision-heavy controller traversal and live resize remain required only before making the feature default ON.
24. M7 has a default-off, single-model Gate 1 PoC that negates the metal-box normals. In a source-matched Dusklight build, first verify Geometry disabled against the previous MidnaFX build. Find a clearly visible `ironbox` instance and record its position. `D_MN04` room 7 loads `L_mbox_00.arc/l_metabox_00.bmd`, but its default spawn is far from the actor. `F_SP116,3,13,2` spawns near three layer-2 `ironbox` actors and loads the model; a temporary draw-suppression test proved the large visible crates are different scenery. A temporary render-only relocation placed one actual metal box in-frame, and matched original/negated captures proved its normal-driven lighting changed. The relocation hook was removed. Capture a naturally unobstructed original view when available. Enable **Mutation test: metal box only** before reloading the scene, confirm exactly one mutation log, and capture the same view with unmistakably changed lighting; the controlled relocated-box proof already passed on Windows. Disable the toggle, reload, and capture the restored view. Verify two instances if available, archive unload/reload, and mod reload. Graceful process exit already logged byte restoration on Windows, but the other lifecycle checks remain open. The supplied June 2026 Dusklight v1.4.1 executable is incompatible with MidnaFX's pinned September 2026 SDK; use a source-matched host. **Log model catalog on resource load** remains an optional read-only aid and must be enabled before the target archive loads.
25. Gate 2 topology validation has passed, and a default-off smoothing experiment is allowlisted for one Forest Temple hanging pot. Matched original/smoothed room-19 captures, five instances sharing one processed resource, in-process toggle restoration, an archive unload/reload cycle through room 0, restoration during Dusklight's native-mod reload, and smoothing reapplication on subsequent archive loads are documented in `geometry-investigation.md`. The small pot capture is preliminary visual evidence only. Before broader use, capture close original/smoothed views for a character's face/body/clothing/armor, an organic or environmental model, and a hard-surface model. Repeat with an animated/skinned character. Inspect hard edges, UV seams, black or inverted lighting, exploding highlights, animation/skinning artifacts, and lifetime-related crashes. Repeat in Twilight and normal-world scenes, with Dawnlight off/on, and with MidnaFX grading/detail enabled. Record full per-model load-time and peak-memory costs. Those remaining checks must not be marked passed by CI.

Matched visual captures for grading, geometry, camera, atmosphere, and depth of field are explicitly deferred
until a controllable desktop session is available. Continue nonvisual build, runtime-log,
lifecycle, and architecture validation, but do not convert those results into visual claims.

For each A–H normal-world screenshot use the same save/camera, HUD state, display
resolution, render scale, Dawnlight state, texture pack, and bloom mode. Capture A and
B before the scene changes. For C and D use identical smoke grading with detail OFF
versus ON; for E use the same settings as D. G contains two files. H contains smoke
and neutral variants. Write these settings and the exact capture sequence into a
`session.txt` beside the PNGs. Keep original PNG files without recompression so later
pixel-difference and histogram analysis can align them. Animated particles, camera
jitter, and temporal bloom can invalidate naive pixel equality even when rendering is
correct.
