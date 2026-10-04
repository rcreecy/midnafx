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
| 3: optical thickness | **INCOMPLETE** | Scene depth is available, but the current exact GX/J3D surface path does not expose a water-fragment mask/surface depth to a programmable pass. |

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

Gate 3 is **INCOMPLETE**. Raw scene depth is available, and representative actors
provide semantic water height, but GX TEV cannot reconstruct world position or
compare behind-water depth with the current water fragment. The programmable
WebGPU pass has no exact water mask or per-fragment surface depth. A framebuffer
color-key mask would be heuristic and is explicitly rejected. No thickness or
product optical code has been added.

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

## Smallest required Gate 3 contract

Gate 2 needs no Dusklight extension. Existing GX EFB-copy semantics provide the
portable, GPU-only color bridge when MidnaFX uses a private texture identity.

Gate 3 needs an exact programmable representation of the water surface. The
smallest generally useful Dusklight extension is either an auxiliary attachment
that exact J3D/GX shape draws can write, or a draw-scoped callback that exposes
the generated water geometry/current transforms to a mod draw. It must preserve
main-pass ordering and expose scene depth plus water-fragment depth without GPU
readback. This would let MidnaFX produce an explicit water mask and reconstruct
thickness in WebGPU. It must define viewport mapping, resize behavior, attachment
clearing, command ordering, and frame-scoped ownership.

MidnaFX should not infer a mask from framebuffer color, retain borrowed views,
or reuse TP's native EFB-copy storage. The next experiment is a Fishing Pond
water-surface depth/mask diagnostic using that explicit contract.

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

Gates 1 and 2 are complete. Gate 3 stops at the missing exact programmable
water-surface depth/mask contract described above.
