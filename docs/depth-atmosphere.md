# Depth atmosphere

Select **Profile / Remaster**, or enable **Depth atmosphere** independently.
Strength defaults to 35%. Existing installations retain their saved switches.
Vanilla disables the effect along with the other profile effects.

The shader reconstructs axial view depth, then blends toward the game's current
fog color with bounded exponential optical depth. Native fog start/end values
control the distance interval. Negative native start distances clamp to zero.
The shader reads the current palette every scene, so room, weather and time-of-day
palette changes flow through without a separate color schedule. This adds to
native fog; it does not replace the game's original fog or add volumetric lights.

The implementation reuses ambient occlusion's opaque-stage snapshot, pipeline
and draw. When only atmosphere is enabled, the shader skips normal reconstruction
and AO's neighborhood samples. With AO enabled, atmosphere composites after
contact shading. Scene alpha/depth remain unchanged; later translucent draws
and the HUD remain outside this pass. This limits treatment of transparent
materials and effects. It is not material-aware relighting.

Atmosphere bypasses underwater camera views, paused game menus, unavailable
player/world input, invalid fog ranges, missing depth and unsupported layouts.
AO can continue independently when atmosphere bypasses. Strength zero avoids
atmosphere work; both effects off avoids the shared draw and snapshots.

Windows D3D12 Ordon observation uses `F_SP103,0,27,0`, the existing external
replacement pack, a 960x720 scene target and a fixed camera. Native inputs are
RGB 30/35/40 with a 0..40000 distance interval. The matched comparison changes
only atmosphere; AO, grading, camera, AA and enhanced water remain enabled.
This scene uses a dark native fog palette, so the added distance veil is subtle.
Compare [disabled](evidence/depth-atmosphere-2026-10-25/off.jpg) and
[enabled](evidence/depth-atmosphere-2026-10-25/on.jpg). These are JPEG captures,
not lossless pixel equality tests. All 21 Windows CTest entries pass for 1.3.0,
including shader compilation, preset persistence and the package contract.
The native preview closes normally with exit code 0.
It is not a claim of broad scene/motion quality or measured GPU performance.
macOS gameplay/Metal execution remains unavailable and support is best effort.
