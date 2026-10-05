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
| 3: optical thickness | **PROVISIONAL** | Exact mask/surface-depth capture and the thickness pass execute in the live runtime. A visible heatmap and native-water replay still require visual confirmation before PASS. |

No optical replacement is enabled. Absorption, animated normal detail,
refraction, Fresnel, reflection, shoreline treatment, specular, and SSR remain
behind these gates.

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
spawn does not show the water surface. The first absorption prototype remains
restricted to the exact validated allowlist and fails closed when inputs are
unavailable. No refraction, animated normals, reflection, or gameplay water
changes are included.

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

The feature remains experimental and default off. Dungeon water still has only
execution evidence because the available Lakebed spawns do not expose a useful
surface view, and underwater transition behavior remains unvalidated.

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
Next, validate dungeon pixels and underwater transitions when a useful
controllable spawn is available, then broaden the allowlist only with matched
visual proof.

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

Gates 1-3 pass for the narrow exact allowlist, including bounded multi-packet
aggregation. Additional-scene product validation remains before broader rollout.
