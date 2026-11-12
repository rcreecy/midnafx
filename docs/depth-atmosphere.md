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

## Terrain mist

Depth atmosphere also adds a low layer near the camera. A cached collision grid
samples ground and water heights in the loaded area. Water raises and thickens
the layer, surrounding higher ground collects it in hollows, and exposed dry
surfaces get a thin veil. The grid blends across banks and low ledges, while
the native wind drifts the density variation. Route thinning uses terrain
exposure as a proxy. It has no stage-specific tuning or separate toggle, and
works in eligible scenes without an authored distance-fog interval.
The low layer thins around Link and Epona as their positions move through it.

## Vegetation particles

Outdoor vegetation now emits drifting pollen in daylight. Qualified forest-floor
vegetation and leaf litter support pulsing fireflies at night. Both use stable
world-space anchors from the existing terrain classifier, shared wind direction
and strength, soft depth-tested sprites, and native fog attenuation. There is no
stage whitelist. Bare paths do not supply anchors; particles can drift across them.

The existing Depth atmosphere switch controls particles. Atmosphere strength zero
also suppresses them. Ground cover can be disabled independently while vegetation
qualification remains available for particles. Underwater, indoor, paused and
Twilight views bypass this pass. Scenes without qualified vegetation emit none.
Fireflies follow the native clock rather than a separate schedule.

The pass selects at most 64 nearby vegetation anchors within 2,200 world units,
draws at most 192 sprites including at most 32 fireflies, and preserves scene
alpha and depth. This is local airborne detail, not simulated insect ecology or
volumetric scattering. Dense terrain reservoirs still require a CPU scan to
select nearby anchors; GPU cost has not been measured.

Windows D3D12 Faron Spring checks show daylight pollen and nighttime fireflies
in the same dialogue camera, with normal close exit 0. Motion tests cover clock
gating, finite bounded trajectories, continuous insect motion and invisible
pollen lifetime resets. See the [particle receipt](evidence/airborne-2026-11-10/README.md).
Whole-game artistic coverage and macOS gameplay remain unverified.

## Canopy sunlight and palette

The contact-shadow control also traces a longer, filtered ray toward native
sunlight through visible scene geometry. Daylit receivers get a warmer response,
and shaded receivers get a cooler response. Sun availability, native weather,
and the outdoor scene determine activation. The ray reaches 1,600 world units
with twenty samples, sharing the existing half-resolution contact field.
Geometry outside the captured view cannot cast these added shadows.

New settings enable the revised Natural color look and restrained texture detail.
Remaster also selects that look and uses brighter midtones and richer saturation.
Saved switches still apply, and the existing color intensity and bypass controls
govern the palette.

## Sunlit atmosphere

Version 1.21 adds a directional daylight tint to the existing fog target color.
It uses the native sun packet's world position, RGB and daylight alpha. The
world position is transformed into a normalized view direction, including
off-screen and behind-camera directions. Source preparation validates a finite
right-handed rigid view and supported perspective projection. No native pointer
or source history reaches the render worker.

A peak-normalized forward lobe with fixed asymmetry 0.65 blends the native fog
target toward native sun RGB. Its blend is capped at half the current daylight
alpha. The original axial distance interval, exponential density and atmosphere
strength remain unchanged. The effect redistributes fog color; it does not add
an independent light-energy term or change the atmosphere cap. Scene alpha stays
exact, including at the peak. Existing contact lighting still composes first.

Native daylight alpha responds to daytime and rain. The sun sprite's separate
visibility is based on projection and local depth probes, so it is intentionally
not used for haze availability. A sun leaving the viewport does not introduce a
source cutoff. Clear native weather, at most ten native snow particles, an
initialized sun packet, non-hidden outdoor sky, non-Twilight and non-event
contexts qualify. Existing player, pause, water, fog-range and viewport gates
remain. Missing, black or invalid sun input keeps the original fog exactly.
Weather/event gates can change discretely; whole-game transition quality is
not established.

This is a bounded LDR directional tint, not a volumetric transport simulation,
height fog, shadowed participating medium or material relighting. Hidden and
transparent geometry do not produce new atmospheric shadowing. Indoors, Twilight,
unsupported weather and unavailable sun data retain the earlier atmosphere.
The implementation adds scalar/vector shader work and 32 uniform bytes without
another texture, compute dispatch or draw. GPU duration and 4K cost remain open.

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

The [1.21 evidence ledger](evidence/sunlit-atmosphere-2026-11-02/README.md)
retains portable source checks and 103 production GPU cases, including sixteen
new directional-haze comparisons. These establish synthetic bounds and source
fallback, not whole-game motion or artistic quality. RDP initially blocks native
capture. After desktop restoration, a [published-package follow-up](evidence/sunlit-atmosphere-2026-11-02/native-release/README.md)
passes native source activation, Ordon visual smoke, toggle at 35%, fullscreen
resize/return, live reload and normal close with exit zero. The captures retain
unfrozen context rather than an isolated old-fog versus sunlit-haze comparison.
Earlier native observations above cover the previous implementation separately.
