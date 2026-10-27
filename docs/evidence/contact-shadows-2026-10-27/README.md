# Directional contact shadows, 2026-10-27

The 1.9.0 slice adds short directional contact shadows across visible opaque
scene depth, without asset identities. See [the contract](../../contact-shadows.md).

## Focused GPU evidence

`gpu-final.log` retains four passing CTest entries on the working Windows D3D12
host: production shader compilation, preset round trips, `ambient_pixels` and
the final 1.9.0 package contract. The GPU fixture now has 20 cases, including all
nine prior AO/fog cases. Shadow checks cover independent strengths, geometric
direction, filtering, depth isolation, alpha, fog order, the 50% combined darkness
cap, zero-strength parity, reversed depth and small/odd targets.

A receiver at view Z=-10 and a raised rectangle at Z=-8.5 are lit from opposite
view-space X positions. AO is disabled. Reversing the light moves the measured
receiver attenuation to the opposite side of the caster:

| Fixture | Darkened receiver pixels, right/left light | Maximum red drop | Weighted X centroid, right/left light |
|---|---:|---:|---:|
| 96×80 | 654 / 659 | 45 / 45 | 36.26 / 58.65 |
| 65×49 | 205 / 189 | 42 / 43 | 23.76 / 38.78 |

The fixtures use strength 40% and a four-unit range to match their geometry.
Counts refer to receiver pixels only. Flat planes and lights behind the surface
retain exact source color. A seeded attenuation field smooths within a plane
without crossing a distant depth step. Zero shadow strength retains the prior
AO color and fields exactly. Matched forward/inverse projection, background and
depth conversion preserve reversed-Z output within bounded rounding tolerance.

Review found that a normal-biased origin retained the original light direction
and distance, allowing a ray to extend beyond a nearby light. The final shader
recomputes direction/range from the biased origin and rejects sampled geometry
outside the segment. A 192×192 close-light receiver fixture with a visible caster
beyond the endpoint passes. No old-build GPU failure is claimed for this fixture;
the original defect was identified by source review. These checks establish
bounded synthetic geometry behavior, not a complete lighting or motion result.

## Native Windows observations

The final local package SHA-256 is
`B225E82AE416CA1616BA348E486129FEFEB8C552CDCC210F21CEDC9B74AEB147`.
The D3D12 baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The isolated session uses stage `F_SP103,0,27,0`, a 960×720 scene target, autosave
off and an empty memory card. Two DDS registrations from
`build/asset-hot-reload-pack/A-first` remain active. This is not complete 4K-pack
verification; no game or texture payload is included here.

`on.jpg` and `off.jpg` share the same camera and simulation frozen with `rate 0`.
Contact shadows alone change at the 30% default. AO, grading, detail, exposure,
bloom, atmosphere, model shading and camera modifiers are off. AA and enhanced
water remain on. `controls.jpg` records shadows enabled independently of AO.
Native water/cursor/HUD animation can continue, and captures are JPEG; they are
not lossless parity proof. The no-controller toast remains present.

Mean RGB code values over fixed screenshot rectangles show localized changes:

| Rectangle (x,y,width,height) | Shadows off | Shadows on |
|---|---:|---:|
| Link region (410,280,140,300) | 77.0957 | 76.9025 |
| Rock region (215,165,140,70) | 83.7865 | 82.6962 |
| Path (550,230,180,100) | 79.7376 | 79.6973 |
| Ground beside right rock (700,390,180,100) | 66.2595 | 66.2595 |
| Hearts (80,85,90,35) | 93.3738 | 93.3738 |

These JPEG averages are not linear-light measurements or proof that every change
is shadow-only. They support a restrained native scene effect rather than global
darkening; the independent GPU fixture establishes directional casting behavior.

Literal `rate 30` resumes simulation. Selecting **Profile / Remaster** re-enables
the combined effects. F11 changes the scene target to 1620×1022, then the native
UI reloads the module. `resized.jpg` records the selected profile at the larger
size; `reloaded.jpg` records continued scene rendering. F11 restores 960×720 in
`restored.jpg`. These are lifecycle observations, not matched art comparisons.
`native-final.log` records 3,263 compute/draw callback pairs before reload and
1,108 afterward, with one retained target set at each shutdown. Those counts
do not measure frame rate or GPU completion time. The saved profile remains
Remaster with contact shadows enabled at 30%.

Normal close exits zero, restores owned geometry and logs `all mods unloaded`.
The empty card reports `Failed to open file: gczelda2`. After mod unload, the host
reports aborted buffer mappings and `Device lost: Device was destroyed.` These
teardown warnings remain in the log; no new shader validation error is reported.
Initial native window activation needed a retry; no failed screenshot was used
as gameplay evidence.

## Remaining limits

No new native dungeon/night/combat/cutscene matrix is claimed. Hidden/off-screen
casters and translucent shadowing remain unsupported. Per-material/local light
selection, whole-game motion/art quality, GPU duration and 4K performance remain
open. macOS gameplay/Metal execution are unverified. Native package builds and
portable release checks continue without hosted Metal runtime tests.
