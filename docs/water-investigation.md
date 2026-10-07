# M12 water modernization investigation

M12 starts after the immutable `v1.0.0` tag. Its first objective is to identify
Twilight Princess water without changing gameplay, geometry, or unrelated
translucent materials. This document separates source evidence from runtime
evidence. A source match or successful build does not pass a runtime gate.

## Gate status

| Gate | Status | Evidence still required |
| --- | --- | --- |
| 1: classify water | **PASS** | Runtime proof covers outdoor, shallow, dungeon, boss, moving/current, and underwater scenes. Unknown paths fail closed. |
| 2: pre-water scene color | **PASS** | The boundary, frame ownership, private GX copy, and consumption by an exact classified water draw are proven at Fishing Pond. |
| 3: optical thickness | **PASS** | Exact mask/surface-depth capture, visible heatmap, native-water preservation, bounded multi-packet replay, and foreground rejection are proven on the exact allowlist. |

The product water path remains default off and exact-allowlist only. Absorption,
animated normal detail, refraction, Fresnel environment fallback, shoreline
treatment, and authored-light specular are implemented behind that boundary.
The rejected SSR prototype does not ship.

## Observed render classes

### Lakebed Temple surface actors

`daLv3Water_c` is the broadest explicit dungeon-water actor. It selects one of
21 archives (`Kr10water`, `Kr10wat01`, `Kr02wat00` through `Kr13wat02`, plus
`Kr03wat05` and `Kr03wat06`). Most variants load a primary surface model and a
second projected-texture model, with independent BTK animation. Types 19 and 20
omit the second model. Actor position supplies a semantic water height while
switch-controlled animation raises or lowers that height.

The primary model enters `DB_XLU_LIST_DARK_BG`. The projected model enters
`DB_*_LIST_INVISIBLE` after receiving a camera projection texture matrix.
`daLv3Water2_c` is a distinct moving Lakebed path using `Kr03wat04`; it draws a
projected animated model only in the invisible list. `daObj_Lv3waterB` uses
`L3_bwater` for Morpheel's arena and also draws through the invisible list. Its
breakable floor is a separate opaque background model and must not be classified
as water.

These actors provide strong identity: actor type, archive, exact model resource,
draw phase, and semantic surface height. They also prove that one visible water
surface may span two J3D models and two draw buffers.

### Generic movable water

`daGrdWater_c` uses archive `Water`, models 17 and 18, two BTKs, two BRKs, and
multiple BCKs. The first model draws through `DB_XLU_LIST_DARK_BG`; the second
uses a projected texture matrix and the invisible list. Its MoveBG collision and
actor position preserve gameplay water-level behavior independently of optical
rendering.

`daObjRotStair_c` follows the same paired-model pattern while its water is
enabled: one dark-background translucent model and one projected invisible
model. This is another useful moving-water classification case.

### Hot springs

`daObjOnsen_c` selects `H_Onsen` or `H_KaOnsen`. Resource 5 is its ordinary
surface/collision model; resource 6 is its BTK-animated projected model. The
second model enters the invisible list. Both pass through TP's background
material processor.

Hot springs are water, but may need a separate visual profile because their
authored color and steam effects differ from ordinary water.

### Stage and background water

Large outdoor bodies such as Lake Hylia are not fully represented by one
water-specific actor. Stage/background J3D materials pass through
`dKy_bg_MAxx_proc`. TP assigns behavior through material-name control codes:

* `MA03`, `MA09`, `MA17`, and `MA19` alter background list selection and fog;
  selected suffixes enter the invisible list.
* `MA10` and `MA02` enter the invisible list and receive camera-projected texture
  matrices.
* `MA00`, `MA01`, `MA04`, and `MA16` alter alpha comparison and depth-write
  behavior when the camera is underwater.
* Other `MAxx` codes control mist, thunder, and environment effects. Therefore
  an `MA` prefix alone is not a safe water classifier.

Exact stage archive, material, shape, texture, blend, and depth-state tuples
must be collected at runtime before this class can pass Gate 1. `F_SP115` room 0
is the Lake Hylia test scene; room 1 is Lanayru Spring. `F_SP112` (Zora's River),
`F_SP126` (Upper Zora's River), and `F_SP127` (Fishing Pond) provide current and
shallow-water candidates.

### Waterfalls, particles, and underwater presentation

`daObjWaterFall` is gameplay collision and flow logic; its `draw()` does not
submit a model. Waterfall visuals therefore belong to stage geometry, another
actor, or particles and cannot be classified from this actor alone.

`daLv3WaterEff_c` owns Lakebed bubble/flow particles. It has no water-surface
model. Existing splash, ripple, bubble, spray, and waterfall particles must stay
outside surface replacement so their actor and scene logic remain intact.

Underwater presentation is also separate. `dKy_undwater_filter_draw` draws an
animated screen-facing environment model when TP reports the camera underwater.
Camera code derives that state from collision water height. Enhanced surface
water must fail closed or use an explicitly defined underwater path; it must not
classify the underwater overlay as a water surface.

## Actual frame ordering

On the PC path, Dusklight emits `GFX_STAGE_SCENE_AFTER_OPAQUE` after opaque
background, dark-background, object, and packet lists. It then draws translucent
background and dark-background lists, followed by ordinary translucent object
lists. Invisible opaque/translucent lists are drawn later, after TP's native
motion-blur and depth-of-field passes.

This places explicit water components in two different parts of the frame:

1. Primary water surfaces in `DB_XLU_LIST_DARK_BG` draw immediately after the
   existing `SCENE_AFTER_OPAQUE` hook.
2. Projected/invisible companion models draw much later.

The public `SCENE_AFTER_OPAQUE` hook is therefore a promising pre-water capture
boundary for primary surfaces. It is not yet sufficient proof for projected
companion models, generic stage water, particles, or later transparent effects.
Gate 2 needs runtime inspection of the sampled result and exact surface draw
ownership.

## Classification design

Initial classification must use positive semantic evidence. Accepted evidence
can include exact actor type, archive/resource identity, exact material name,
shape association, and observed draw buffer. Blend mode or translucency alone is
insufficient. Framebuffer color matching is prohibited.

The first diagnostic will use a small exact allowlist. It will record one
machine-readable row per classified model/material with:

* stage, room, layer, and actor class where available;
* archive and model resource identity;
* material and shape index/name;
* blend mode and factors;
* depth test, comparison, and write state;
* texture indices and texture-matrix animation presence;
* selected draw list;
* semantic water height where available; and
* diagnostic draw count and CPU cost.

The visual proof will make only classified surface shapes unmistakably magenta.
The diagnostic does not mutate J3D material or model data. It replaces GX TEV
state immediately before the selected shape draw; subsequent material display
lists establish their own state. The feature remains developer-only and default
off. Unknown resources, missing services, unexpected fingerprints, unsupported
material layouts, and underwater-only overlays fail closed.

## Runtime evidence checkpoint

The diagnostic runs in the source-matched Windows D3D12 host and is available
through `water_classification_diagnostic`. It defaults off. Six explicit water
actor hooks resolved at runtime. Stage background discovery uses the active
`daBg_c` and `daBgObj_c` process instances because the small profile draw wrappers
are not stable public hook targets.

Observed exact stage materials:

| Scene | Model part | Selected material | GX evidence |
| --- | --- | --- | --- |
| `F_SP115`, room 0 | stage part 0 | `cc_MA06_NigoriWater_v_x` | blend `1/4/5`, depth `1/3/0`, 2 TEV stages |
| `F_SP115`, room 0 | stage part 0 | `cd_MA09_MeraWater_v_x` | blend `1/2/0`, depth `1/3/0`, 2 TEV stages |
| `F_SP127`, room 0 | stage part 1 | `cc_MA02_IndirectWater_v` | blend `1/4/5`, depth `1/3/0`, 1 TEV stage |
| `F_SP127`, room 0 | stage part 4 | `cc_MA06_Nigori_Water_v_x` | blend `1/4/5`, depth `1/3/0`, 3 TEV stages |
| `F_SP127`, room 0 | stage part 4 | `cc_MA09_Nigori_Water_v` | blend `1/2/0`, depth `1/3/0`, 2 TEV stages |

`F_SP127` runtime proof marks the full Fishing Pond surface solid magenta while
foreground soil, cliffs, HUD, and nearby scene geometry remain unchanged. Local
evidence is `build/m12-water-lakebed/fishing-pond-magenta.png`.

`F_SP115` start point 20 marks only the visible Lake Hylia water patch while the
wooden platform and surrounding rock remain unchanged. Local evidence is
`build/m12-water-lakebed/lake-hylia-start20.png`. Start point 0 does not face the
lake, so it is not useful visual evidence despite loading the same classified
materials.

`R_SP107`, room 1, start point 2, layer 13 exercises the generic `Water00`
actor. Its primary model contains `cc_MA06_nigori_v_x`, `dd_MA09_mera_v`, and
two `ee_MA03_ryusui_v_x` materials; its projected companion contains
`cc_MA02_waterb_v`. The solid-magenta diagnostic followed the complete moving
surface while Link was swimming. Stone channels, gates, Link, the map, and the
underwater presentation remained unmarked. A matched diagnostic/original pair
is stored locally as `build/m12-water-lakebed/sewers-moving-water-start2.png`
and `build/m12-water-lakebed/sewers-moving-water-start2-original.png`.

Earlier explicit-actor proofs remain valid:

* `D_MN01`, room 3: primary and projected Lakebed water models marked; nearby
  dungeon geometry remained unchanged.
* `D_MN01A`, room 50: Morpheel arena water marked; Link, arena floor, and swim
  opening remained unchanged. Local evidence is
  `build/m12-water-lakebed/morpheel-point0-magenta.png`.

No D3D12/WebGPU validation errors occurred in these runs. Normal game-card
warning `Failed to open file: gczelda2` remains unrelated. Graceful shutdown
uninstalled all diagnostic hooks and did not require material restoration because
no model/material bytes were changed.

A final Fishing Pond run marked 1,845 shape draws across two classified models.
Stage actor discovery consumed 6,472 microseconds over 643 frames, about 10.1
microseconds per frame while the developer diagnostic was enabled. This scan and
all material-name work are bypassed while the default-off diagnostic is disabled.

Gate 1 is **PASS** for the representative classes required to choose exact water
surfaces. Proof covers dungeon, boss, Lake Hylia, Fishing Pond, generic moving
water, and an underwater/swimming state. The Fishing Pond stage also contains
the distinct `cd_MA03_TakiKasan_v_x` waterfall material. It is excluded from the
water-surface allowlist, consistent with source evidence that `daObjWaterFall`
owns flow/gameplay behavior but submits no surface model. Hot springs remain a
catalogued, story-layer-gated coverage case rather than a prerequisite for the
first optical prototype. Unrecognized actors and stage materials remain native.

## Gate 2 runtime evidence

The default-off `water_scene_capture_diagnostic` resolves color and depth at
`GFX_STAGE_SCENE_AFTER_OPAQUE`, retains the borrowed views for the current frame
only, and presents the captured color at `GFX_STAGE_FRAME_BEFORE_HUD`. A
`GFX_STAGE_SCENE_BEGIN` callback clears all borrowed handles so an early return
cannot carry stale views into another frame.

At `F_SP127`, room 0, the diagnostic displayed the opaque terrain and submerged
geometry behind the Fishing Pond surface. The native water surface was absent,
which proves it was not recursively included. Link and opaque scene geometry
were present; the HUD rendered afterward and remained live. The captured color
was 1216x896 and matching raw depth was available. Local evidence is
`build/m12-water-lakebed/fishing-pond-prewater-capture.png`.

Resizing the host window to 960x720 rebuilt the scene layout and continued to
display the correct pre-water image without stretching, stale views, a crash,
or a WebGPU/D3D12 validation error. Local evidence is
`build/m12-water-lakebed/fishing-pond-prewater-resized.png`. The ordinary card
warning `Failed to open file: gczelda2` remained the only error-level message.

The remaining consumption proof uses Aurora's existing GPU-resident GX EFB-copy
path rather than importing a borrowed WebGPU view. At
`GFX_STAGE_SCENE_AFTER_OPAQUE`, MidnaFX copies the current scene into a private,
stable texture identity. Exact classified `J3DShapePacket::drawFast` calls bind
that texture to `GX_TEXMAP0` and sample it using the water material's existing
first texture coordinate. The diagnostic does not read back, retain a borrowed
view, overwrite a TP framebuffer-copy identity, or classify framebuffer colors.
Its stage hooks exist only while a water diagnostic is enabled; resize recreates
the private copy and disable/shutdown destroys it.

The source-matched Windows runtime recorded:

* `Water surface capture ready {size=608x448 format=RGBA8}` (logical GX size;
  Aurora scales the copy to the active render target),
* `Water surface capture sampled {material=... size=608x448}` on an exact
  Fishing Pond allowlisted material,
* 11,235 classified surface draws over 3,773 frames,
* 35,855 microseconds total stage scan time, about 9.5 microseconds per enabled
  frame,
* clean mod unload and no WebGPU/D3D12 validation errors.

Combined with the matched 1216x896 pre-water screenshot above, this proves that
the exact water draw consumes the opaque scene captured before water. Gate 2 is
**PASS**.

## Gate 3 runtime evidence

The default-off `water_thickness_diagnostic` now creates an exact programmable
surface representation without changing model data. MidnaFX records unique exact
classified `J3DShapePacket::drawFast` packets while their native draws proceed
untouched. Immediately before its fullscreen stage, it replays all collected
packets into one private full-resolution target with solid-white coverage and
depth writes, then resolves the combined mask plus `R32F` water-surface depth.
The fullscreen diagnostic compares this surface depth with raw opaque depth from
`GFX_STAGE_SCENE_AFTER_OPAQUE`, reconstructs view distances with the current
camera matrices, and displays clamped optical thickness as grayscale.

The source-matched Windows D3D12 runtime reached this path at `F_SP127`, room 0,
for the exact `cc_MA02_IndirectWater_v` Fishing Pond surface. It recorded:

* `Water thickness pre-depth {ready=yes requested=608x448 resolved=1216x896 depth=yes}`;
* `Water thickness surface capture ready {size=1216x896 mask=color depth=R32F}`;
* `Water thickness diagnostic active: size=1216x896 max_depth=5000 reversed_z=yes`;
* clean `all mods unloaded`; and
* no WebGPU, D3D12, device-lost, or validation errors.

A repeat run measured 347 successful surface captures with zero capture failures.
The shape capture, resolve submission, state restoration, and native replay used
75,257 microseconds of CPU hook wall time in total, about 217 microseconds per
captured frame. This is submission-side CPU time and does not claim GPU duration.
The diagnostic remains developer-only and performs none of this work while off.

The same source-matched runtime path also executed on three additional classes:

| Class / scene | Successful captures | Failures | CPU hook wall time |
| --- | ---: | ---: | ---: |
| Large outdoor: Lake Hylia `F_SP115` | 260 | 0 | 24,561 us total / 94 us each |
| Moving/swimmable: `R_SP107` generic water | 260 | 0 | 26,083 us total / 100 us each |
| Dungeon: Lakebed Temple `D_MN01` | 259 | 0 | 25,196 us total / 97 us each |

All three runs unloaded cleanly with no WebGPU/D3D12 validation or device-loss
errors. This proves that each class reaches the narrow one-shape capture path; it
does not prove complete multi-shape coverage or visual correctness.

The first run exposed an important viewport distinction: GX reported a logical
608x448 size while the active target and depth view were 1216x896. The diagnostic
now uses the resolved target dimensions for its offscreen attachments and
viewport. All borrowed pre-depth views are cleared at the next frame boundary;
private surface targets are recreated on resize and destroyed on disable or
shutdown. Missing services, views, camera matrices, or unsupported formats bypass
the diagnostic.

Direct Fishing Pond observation confirmed the water
surface is isolated as grayscale, shallow/deep structure changes across the
surface, the HUD remains live, and disabling the diagnostic restores native
color. Local evidence is
`build/m12-water-surface/fishing-pond-thickness-stable.png`.

The first visual run exposed alternating grayscale/native-color frames. Stage
background classification had run only on 30 Hz simulation ticks and was cleared
after each presentation, so Dusklight's interpolated presentation frames lacked
classification. Stage actors are now rescanned at `SCENE_AFTER_OPAQUE` on every
presentation frame while a capture diagnostic is active. The follow-up ran
10,006 captures with zero failures and no visible flicker; scan cost averaged
about 24 microseconds and capture/replay CPU hook wall time about 139 microseconds
per frame. It shut down cleanly without validation errors.

The first moving-water attempt selected `ee_MA03_ryusui_v_x(2)`, a current/overlay
layer whose isolated shape produced no visible mask. Thickness capture now keeps
the broader exact classifier for diagnostics while selecting only the base
surface: material zero on explicit primary/surface actors, `MA06` stage water,
or the exact Fishing Pond `cc_MA02_IndirectWater_v`. In `R_SP107` this selected
`cc_MA06_nigori_v_x`. The corrected view shows a continuous grayscale thickness
field on the visible moving water. Stone banks and Link are black because opaque
depth in front of the water plane is rejected; no red or magenta reconstruction
faults appear. Local evidence is
`build/m12-water-surface/moving-water-thickness-occlusion-correct.png`. The run
completed 6,471 captures with zero failures, about 90 microseconds of CPU hook
wall time per frame, and clean unload.

An unattended Lake Hylia run also displayed a stable water-local grayscale field
with no red/magenta reconstruction faults, then unloaded cleanly after 688
captures with zero failures. Lakebed Temple room 3 still provides execution-only
evidence: its three valid spawn points do not present a useful visible water
surface, so no visual claim is made for that scene.

Gate 3 is **PASS** for deriving stable optical thickness on the validated exact
allowlist. Evidence now covers Fishing Pond stage water, large
outdoor Lake Hylia water, and generic moving/swimmable actor water. Background
depth is handled separately, invalid reconstruction is conspicuous, and opaque
geometry in front of the classified plane cannot contaminate thickness.

Multi-packet aggregation is also implemented and bounded to 64 unique exact
packets per frame; overflow fails the frame closed. Lakebed Temple room 3
exercised four packets per frame for 1,464 combined captures with zero failures,
about 52 microseconds of CPU replay work per frame, clean unload, and no
validation errors. This is execution and lifecycle proof because the available
spawn does not show the water surface. Product water effects remain restricted
to the exact validated allowlist and fail closed when inputs are unavailable.
Reflection and gameplay water changes are not included.

## First product feature: depth-based absorption

The default-off `Enhanced water (experimental)` setting applies a restrained
Beer-Lambert-inspired pass after the narrow pre-water capture. It uses the water
mask, reconstructed surface depth, opaque scene depth, and current scene color.
Missing inputs, invalid/background depth, and foreground occlusion bypass the
effect. Strength defaults to 0.65 and maximum optical depth to 5000, with muted
green-blue shallow/deep tints. Runtime proof reached the absorption pipeline on
R_SP107 without shader validation errors or red/magenta diagnostics. Two defects
were found during the first visual run: normal grade selection overwrote the
absorption pipeline, and the auxiliary surface pass ran before the native water
draw, so replay lost authored water color. Pipeline selection now preserves the
absorption kind. Native water draws first; the same geometry is replayed only
into the private mask/depth pass afterward. A zero-strength control visually
matches native water, including animated distortion and transparency. Default
strength adds restrained depth tint while retaining those authored details.
Evidence is `build/m12-water-surface/absorption-default-off.jpg`,
`absorption-zero-inverted.jpg`, and `absorption-default-inverted.jpg`. The final
run completed 1,449 captures with zero failures, about 88 microseconds of CPU
capture/replay wall time per frame, clean unload, and no WebGPU/D3D12 validation
errors.

Matched zero-strength/default-strength runs now also pass on Fishing Pond and
Lake Hylia. Fishing Pond provides the clearest product proof: the default pass
adds a darker depth-dependent tint across the broad visible pond while retaining
the native animated surface, transparency, shoreline, and background geometry.
Its zero/default runs completed 1,171/1,143 captures with zero failures, about
138/147 microseconds of CPU capture/replay wall time per frame, clean unload,
and no validation errors. Lake Hylia's start-point-20 view exposes only a small
water patch, but both runs remained visually stable and retained native surface
detail; they completed 1,334/1,038 captures with zero failures at about 86/85
microseconds per frame. Evidence is
`build/m12-water-surface/absorption-fishing-zero.jpg`,
`absorption-fishing-default.jpg`, `absorption-lake-zero.jpg`, and
`absorption-lake-default.jpg`.

The feature remains experimental and default off. A matched native/enhanced
R_SP107 room-1 surface-swimming check retained Link, the native animated
surface, transparency, HUD, and surrounding geometry. The enhanced run then
remained stable for 392,535 one-packet captures with zero failures, about 251
microseconds of CPU capture/replay work per frame, and clean unload; a shorter
repeat completed 3,134 captures with zero failures at about 219 microseconds per
frame. Evidence is `build/m12-water-surface/underwater-surface-native.png` and
`underwater-surface-enhanced.png`. This validates swimming at the air/water
boundary.

Flycam validation subsequently exposed the Lakebed room-3 surface and crossed
it from both sides. Enhanced water retained the dungeon's authored surface,
surrounding geometry, submerged fish, underwater distortion, and the visible
surface boundary. Repeated air-to-water-to-air movement remained stable. Native
and enhanced evidence is `build/m12-water-surface/dungeon-native.jpg`,
`dungeon-enhanced.jpg`, `dungeon-underwater-native.jpg`, and
`dungeon-underwater-enhanced.jpg`. This proves camera-state rendering across
the surface; it does not replace a gameplay dive/swim-input test.

An earlier all-features Lakebed room-3 run processed four exact packets per
frame for 11,900 captures with zero failures, `light=yes`, and clean unload. It
measured about 1.61 milliseconds of CPU capture/replay work per frame. A
specular-off repeat measured 1.437 milliseconds per frame, proving the cost was
in surface replay rather than the optical shader.

Review found that the auxiliary replay redundantly called `prepareDraw()` before
`J3DShapePacket::drawFast()`, which calls it itself, and reloaded the source
material even though the mask pass is untextured. The material reload rebound
textures from a stale J3D texture table and produced two out-of-range texture
lookups per packet per frame. Removing both redundant operations reduced the
same four-packet Lakebed run to 198,970 microseconds over 3,678 captures, or
about 54 microseconds per frame. It had zero capture failures, zero texture-index
errors, normal rendering, and clean unload. A visible R_SP107 surface-swimming
repeat remained visually correct and measured 42,400 microseconds over 1,002
one-packet captures, about 42 microseconds per frame. Evidence is
`build/m12-water-surface/optimized-swim-enhanced.jpg`. The later visible
Lakebed flycam run completed 17,205 captures with zero failures and four packets
per frame at about 53 microseconds of CPU replay work per frame. Its dedicated
underwater transition repeat completed 9,361 captures with zero failures at
about 53 microseconds per frame. Both unloaded cleanly with no texture-index or
WebGPU/D3D12 validation errors. GPU shader timing remains open; the resolved CPU
replay variance no longer blocks dungeon enablement by itself.

## Animated surface-normal prototype

Enhanced water now derives one shared procedural surface normal from two
independently scrolling, opposing wave layers. The coordinates are reconstructed
in world space from exact water-surface depth and `world_from_proj`, so the
pattern remains attached to the water instead of the screen. A large slow layer
provides broad structure and a smaller opposing layer breaks repetition. The
combined normal currently supplies restrained moving light variation to the
absorption result and is the input intended for later refraction and Fresnel.

One persisted `Water surface detail strength` control ranges from 0-100% and
defaults to 20%. Fishing Pond was tested at 0%, 100%, and the 20% default. The
maximum diagnostic made the continuous two-layer motion observable without
affecting shore, terrain, Link, HUD, or unrelated translucency; the default was
subtle and retained TP's authored surface texture. The default run completed
1,289 combined captures with zero failures, two packets per frame, about 51
microseconds of CPU replay work per frame, clean unload, and no validation
errors. The additional shader cost has no GPU timestamp measurement yet.
Evidence is `build/m12-water-surface/waves-fishing-zero.jpg`,
`waves-fishing-maximum-a.jpg`, `waves-fishing-maximum-b.jpg`, and
`waves-fishing-default.jpg`.

## Bounded screen-space refraction prototype

Enhanced water now optionally samples the same pre-water opaque scene-color
resolve used with the depth capture. The animated world-space surface normal is
transformed into view space and offsets sampling by at most eight pixels. The
offset grows with optical thickness, clamps to the viewport, rejects background
depth and opaque geometry in front of the water, and blends at no more than 35%
so TP's authored surface remains visible. Refraction strength is persisted,
bounded from 0-100%, and defaults to 15% under the default-off enhanced-water
toggle.

Fishing Pond passed zero, default-15%, and maximum-100% runs on D3D12. Zero
retained the established absorption result. Default remained restrained.
Maximum made the water-local distortion observable without affecting Link,
shore terrain, HUD, or unrelated translucency; off-screen sampling and gross
edge smearing were not observed. The maximum run completed 3,164 combined
captures with zero failures, two packets per frame, about 45 microseconds of CPU
replay work per frame, clean unload, and no WebGPU/D3D12 validation errors.
Evidence is `build/m12-water-surface/refraction-fishing-zero.jpg`,
`refraction-fishing-default.jpg`, and `refraction-fishing-maximum.jpg`. The
Lakebed surface and submerged flycam passes later exercised this same combined
water path without recursive image, edge-smear, or transition failure. GPU cost
still lacks timestamp measurement.

## Fresnel and environment-reflection fallback

Enhanced water now applies a Schlick Fresnel term after transmission,
absorption, and scattering. The animated surface normal and reconstructed view
position produce the view angle. Near-normal views retain transmission while
grazing views blend toward a restrained configurable environment tint. This is
an explicit fallback rather than scene reflection: no sky/environment semantic
is currently exposed. Reflection strength defaults to 25%; tint defaults to a
muted blue-green and both strength and RGB channels are persisted controls.
The blend is capped at 45%, invalid vectors fail closed, and the whole path
remains behind the exact allowlist and default-off enhanced-water toggle.

Fishing Pond passed a 100% diagnostic run. The grazing water band gained the
expected cool response while near-normal portions remained transmissive; Link,
shore terrain, HUD, and unrelated translucency were unaffected. The run
completed 3,123 captures with zero failures, two packets per frame, about 44
microseconds of CPU replay work per frame, clean unload, and no WebGPU/D3D12
validation errors. Evidence is
`build/m12-water-surface/reflection-fishing-maximum-fresh.jpg`. The prior
`refraction-fishing-default.jpg` capture is the same-spawn no-reflection baseline.
The fallback is intentionally not described as reflecting real scene content.

## Depth-aware shoreline treatment

Enhanced water now derives a shallow-intersection weight from the same validated
optical thickness used by absorption. Within the shallowest 8% of configured
optical depth it gently blends toward the existing shallow-water tint, modulated
by the shared animated surface normal. The blend is capped at 16%; the default
strength is 20%. It does not synthesize foam, change alpha, alter geometry, or
replace TP's authored ripple and splash effects. Invalid depth, foreground
occlusion, missing masks, and non-allowlisted water continue to bypass the
entire effect.

Fishing Pond passed 0% and 100% runs. The maximum diagnostic produced a subtle
brighter shallow transition without a hard halo or ocean-foam look. Deep water,
Link, shore terrain, HUD, and unrelated translucency remained stable. The
maximum run completed 4,702 captures with zero failures, two packets per frame,
about 51 microseconds of CPU replay work per frame, clean unload, and no
WebGPU/D3D12 validation errors. The zero control completed 4,857 captures with
zero failures at about 56 microseconds per frame and also unloaded cleanly.
Evidence is `build/m12-water-surface/shoreline-fishing-maximum.jpg` and
`shoreline-fishing-zero.jpg`. Lakebed dungeon edges and the irregular Lake
Hylia shore are validated below.

## Environment-aware specular response

TP exposes a stable read-only light position through
`g_env_light.base_light.mPosition`. `dScnKy_env_light_c::SetBaseLight()` selects
this value from the authored sun, moon, or stage light according to the game's
existing environment rules. TP's weather renderer also consumes the same base
light position. MidnaFX therefore uses this semantic signal instead of inventing
a global sun direction. Missing or non-finite light data disables the specular
term for that frame.

Enhanced water now applies a restrained Blinn-Phong highlight from this light,
the reconstructed water position, camera matrices, and the shared animated
surface normal. Strength is persisted from 0-100% and defaults to 12%. The
100% diagnostic remains capped to a 28% additive contribution. Zero length and
NaN vectors return the pre-specular color. The feature remains inside the exact
water mask and default-off enhanced-water path.

Fishing Pond passed matched 0% and 100% D3D12 runs. Runtime diagnostics reported
`light=yes` in both runs. The maximum run produced a restrained water-local
highlight response without affecting Link, shore terrain, HUD, or unrelated
translucency. It completed 7,199 captures with zero failures, two packets per
frame, about 50 microseconds of CPU replay work per frame, clean unload, and no
validation errors. The zero control completed 6,211 captures with zero failures
at about 56 microseconds per frame and also unloaded cleanly. Evidence is
`build/m12-water-surface/specular-fishing-maximum.jpg` and
`specular-fishing-zero.jpg`.

Outdoor light-state stability also passed a controlled Fishing Pond run. The
live console set time to noon (`180`), night (`330`), and directly across the
native sun/moon selection boundary (`67` then `68`). The authored scene light
and shadow changed at each state while enhanced water remained stable: no
exploding, inverted, or blown-out specular response, no NaN/Inf state, and no
WebGPU/D3D12 validation errors were observed. The run completed 16,478 captures
with zero failures, about 54 microseconds of CPU replay work per capture, and a
clean mod unload. Evidence is
`build/m12-water-surface/specular-fishing-noon.jpg`,
`specular-fishing-night.jpg`, `specular-fishing-time067.jpg`, and
`specular-fishing-time068.jpg`.

A matched Lakebed room-3 flycam run then held the underwater camera fixed while
the live console changed TP time from night (`330`) to noon (`180`). The central
world crop increased from 49.55 to 54.62 mean luma (about 10.2%) while the water
surface, submerged geometry, fish, and distortion remained stable. The run
completed 1,399,500 captures with zero failures, four packets per frame, about
47.5 microseconds of CPU capture/replay work per frame, clean mod unload, and no
texture-index or WebGPU/D3D12 validation errors. Evidence is
`build/m12-water-surface/dungeon-light-night.jpg` and
`dungeon-light-noon.jpg`. This proves stability across a controlled authored
dungeon lighting-state change; the exact `base_light` vector was not logged, so
the captures do not claim a measured direction change.

GPU shader duration cannot be measured through the current source-matched public
mod contract. Dusklight's public `GfxService` exposes frame callbacks and borrowed
device/queue access but no asynchronous timestamp result, while Aurora's internal
timestamp profiler is compiled only with `TRACY_ENABLE`; this runtime reports no
`TimestampQuery` feature. CPU submission/capture time is therefore reported
separately and must not be treated as GPU duration. The smallest useful host
addition is an optional asynchronous per-pass timing scope/result exposed to mods,
with no GPU wait or readback on the render path.

## Optional SSR experiment

A bounded eight-step screen-space reflection prototype reused the existing
pre-water scene color/depth plus `proj_from_view`, `view_from_proj`, and the
shared animated normal. It rejected off-screen projection, background depth,
invalid clip coordinates, and samples in front of the water. Misses retained
the environment-tint fallback. CPU capture/replay cost remained about 64
microseconds per frame across 7,750 captures with zero failures, clean unload,
and no validation errors.

The Fishing Pond hit diagnostic rendered every classified water pixel black and
no hit pixel magenta. The tested radial-distance crossing rule therefore found
zero trustworthy intersections in this view. Evidence is
`build/m12-water-surface/ssr-hit-diagnostic-no-hits.jpg`. The prototype was
reverted and no SSR setting or shader path ships from this experiment.

A follow-up miss-reason diagnostic replaced radial distance with signed
view-space depth and separated valid-depth no-crossing rays (yellow), off-screen
rays (red), and background samples (blue). Fishing Pond produced only yellow and
red regions, including after increasing the bounded eight-step range and hit
thickness. It still produced no magenta hits. Evidence is
`build/m12-water-surface/ssr-depth-miss-reasons.jpg` and
`ssr-signed-depth-no-hits.jpg`.

No new Dusklight API is indicated; current scene color, depth, and camera
matrices are sufficient. Any later SSR work needs a more robust screen-space
intersection method, likely perspective-correct segment refinement or a depth
pyramid, plus proof in a scene containing strong reflected silhouettes. SSR
remains optional and must not replace the validated environment fallback until
real hits are proven.

## Fishing Pond submerged-depth quality correction

A controlled `F_SP127`, room 0 comparison exposed a product-quality defect in
the first absorption implementation. At time `180`, FOV `61.651`, camera eye
`(-1748.550, 2071.123, 6018.771)`, and center
`(-1748.550, 2033.058, 5926.299)`, the uncapped Beer-Lambert/scatter result
converted coarse submerged depth discontinuities into large hard-edged brown
silhouettes that were absent from the native surface. Evidence is
`build/m12-water-surface/waterfall-adjacent-enhanced-time180.png` beside
`waterfall-adjacent-native-time180.png`.

The shader now limits absorption/scatter to a 15% contribution over TP's
authored transmitted water color. Refraction, animated normals, Fresnel,
shoreline treatment, and specular keep their independent controls. A 45%
intermediate reduced the defect but still exposed the coarse triangles; the
15% repeat removed the large blocky silhouettes while preserving restrained
depth variation. Final evidence is
`build/m12-water-surface/waterfall-adjacent-enhanced-final-time180.png`.

The final D3D12 run processed 8,919 thickness captures with zero failures,
two packets per frame, and 425,936 microseconds of capture/replay CPU time
(about 47.8 microseconds per capture). It reported no WebGPU/D3D12 validation
errors and unloaded all mods cleanly. Exact classification continued to include
the three Fishing Pond surface materials while excluding the adjacent
`cd_MA03_TakiKasan_v_x` waterfall material.

## Lake Hylia irregular-shore validation

`F_SP115`, room 0, start point 20 provides an irregular rock-water boundary
beside a wooden platform. The final 15% absorption/scatter build kept the
effect inside the authored water surface: the curved rock boundary stayed
clean, and the rock, platform, Link, HUD, and nearby square environmental
particles were unchanged. No red/magenta reconstruction faults, hard depth
silhouettes, or shoreline halos were visible. Evidence is
`build/m12-water-surface/natural-shore-enhanced-final.png`, compared with the
established control `absorption-lake-zero.jpg`.

The D3D12 run captured 3,180 frames with zero failures and one exact packet per
frame. Capture/replay CPU time was 127,120 microseconds total, about 40.0
microseconds per capture. It reported no WebGPU/D3D12 validation errors and
unloaded all mods cleanly. The selected stage materials remained
`cc_MA06_NigoriWater_v_x` and `cd_MA09_MeraWater_v_x`; unrelated translucent
materials remained outside the exact allowlist.

## Test matrix for Gate 1

| Class | Candidate | Required observation |
| --- | --- | --- |
| Large outdoor | `F_SP115`, room 0 | Lake surface marked; sky, mist, distant haze, particles, and other translucency unchanged. |
| Shallow/current | `F_SP112`, `F_SP126`, or `F_SP127` | Moving/shallow surface marked without spray or shoreline effects. |
| Dungeon | one `daLv3Water_c` Lakebed room | Primary and companion model relationship recorded; unrelated dungeon translucency unchanged. |
| Moving water | `daGrdWater_c`, `daLv3Water2_c`, or rotating-stair water | Classification remains attached while water height/geometry moves. |
| Boss water | `D_MN01A` / `L3_bwater` | Water marked; Morpheel arena floor remains unmarked. |
| Underwater | any stable swimmable scene | Surface identity remains stable; underwater overlay and particles remain unmarked. |
| Waterfall adjacent | stage with `daObjWaterFall` | Surface classification does not consume waterfall gameplay actor or unrelated spray. |

## Gate 3 contract and next experiment

Gate 2 needs no Dusklight extension. Existing GX EFB-copy semantics provide the
portable, GPU-only color bridge when MidnaFX uses a private texture identity.

The narrow Fishing Pond experiment needs no Dusklight extension. Existing public
stage callbacks, GX state, and exact shape interception can produce a private
mask/surface-depth pair without readback. This approach is acceptable only if
visual testing proves that replay restores the native water draw exactly.

If replay is not pixel-correct, or if aggregating all shapes/classes cannot be
done safely, the smallest generally useful Dusklight extension remains an
auxiliary attachment for exact J3D/GX draws or a draw-scoped callback exposing
generated geometry and current transforms. Such an API must define viewport
mapping, resize behavior, clearing, command ordering, and frame-scoped ownership.

MidnaFX does not infer a mask from framebuffer color, retain borrowed views, or
reuse TP's native EFB-copy storage. Moving-water, Fishing Pond, and Lake Hylia
A/B now confirm the post-native auxiliary replay preserves authored water at
zero absorption and applies the default tint only inside the classified surface.
Fishing Pond also validates bounded screen-space refraction, a restrained
Fresnel environment fallback, shoreline treatment, and environment-aware
specular on the exact allowlist. Lakebed room-3 flycam proof validates visible
dungeon pixels, a fully submerged camera, repeated surface crossings, and a
controlled authored light-state change. Next, expand exact-class pixel coverage
for any newly added water identity before enabling it. GPU timing requires the
documented optional host instrumentation. SSR remains separate and optional.

## Open questions

* Additional exact identities for story-layer-gated hot springs and rotating-stair water.
* Per-class GX blend, depth test/write, textures, and animated texture state.
* Whether primary and projected models can be replaced as one optical surface
  without double composition.
* Whether `SCENE_AFTER_OPAQUE` depth contains submerged opaque geometry for all
  target scenes.
* Correct treatment of invisible-list water relative to TP's native blur/DOF.
* Whether semantic collision water planes align closely enough with rendered
  geometry for stable thickness reconstruction.

Gates 1-3 passed first for the narrow exact allowlist, including bounded
multi-packet aggregation. The following confidence pass defines the broader
source-enumerated rollout boundary.

## World surface inventory and exact expansion

The stage archive inventory now covers all 79 extracted stage directories. The
read-only `tools/scan-water-materials.py` pass found 229 water-like stage material
rows; `tools/scan-stage-spawns.py` found 1,277 usable runtime starts. Manual
classification retained 110 exact `(stage, room, actor role, material)` identities
across 26 stages and 43 rooms. Seventy-three identities are primary optical-depth
candidates; companion shimmer/indirect layers remain classified but do not cause a
second thickness replay. Six existing actor-specific classifiers continue to cover
Lakebed, ground/current, hot-spring, rotating-stair, and boss water.

The expansion remains an exact allowlist. It deliberately excludes waterfall and
cascade materials, fountain spray, sunbeams, oil, aquarium glass, debug materials,
and particle effects. These are authored effects or different optical classes and
must not be passed through the flat-water replacement path. Unsupported or unknown
stage/material combinations still fail closed.

Source-matched D3D12 magenta runs supplied representative live confidence checks:

| Scene | Result | Runtime evidence |
| --- | --- | --- |
| `F_SP103,0,27,0` | Small Ordon water patches marked; terrain, vegetation, actors, and HUD remained native. | 2,114 marked draws over 1,085 scanned frames; 53,463 microseconds total scan time. |
| `F_SP108,1,0,0` | Broad forest water marked; Midna, Link, shore plants, dialogue, and HUD remained unaffected. | 2,169 marked draws over 751 frames; 28,988 microseconds total scan time. |
| `F_SP117,3,1,0` | Exact large-water materials processed without marking visible room geometry. The selected surface was outside the initial camera view. | 4,968 marked draws over 1,684 frames; 87,530 microseconds total scan time. |
| `D_MN01,0,1,0` | Exact Lakebed static layers processed; the initial camera showed no unrelated magenta geometry. | 577 marked draws over 1,000 frames; 19,519 microseconds total scan time. |
| `F_SP122,17,0,0` | Water beneath the bridge marked; bridge, room geometry, Link, and HUD remained native. | 2,325 marked draws over 803 frames; 24,695 microseconds total scan time. |
| `F_SP118,2,0,0` | Desert-lake model identity observed exactly as room 2 / `part-0`; its surface was outside the initial camera view. | Runtime material `Desert_Lake_051214a_cc_MA02_water_v_x`; actor room and stay room both 2. |

All runs shut down through the normal window close path and unloaded all mods. No
WebGPU/D3D12 validation error was observed; the expected device-destroyed warning
occurred during host shutdown. Scanning cost in the five drawing scenes was about
19.5-52.0 microseconds per scanned frame. The feature remains default off, and
there is no GPU readback or per-frame diagnostic logging after initial model
discovery.

This is the viable broad stage-surface boundary for the current architecture. It
does not claim waterfalls, spray, oil, glass, or every story-layer actor as ordinary
water. Newly discovered water identities still require exact catalog and live pixel
proof before entry.
