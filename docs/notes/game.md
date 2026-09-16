# Game rendering and Twilight evidence

Inspected 2026-09-16. Dusklight checkout `upstream/dusklight`, revision `edf42c6a7202647b56dd2fcdef02d17671bc814b`; Dawnlight checkout `upstream/dawnlight`, revision `74b0351c1aa647a4e1945c62a1192006fa50f713`. Paths below are relative to those checkout roots. This is source evidence, not a rendered comparison. No game executable, assets, save, GPU capture, or visual A/B was used.

## Semantic detection

- `src/d/d_kankyo.cpp:11055`, `dKy_darkworld_check()`, tests `dComIfGp_world_dark_get() == TRUE`. Treat that as equality to the active state, not general nonzero truthiness.
- `src/d/d_s_play.cpp:1233` sets the world-dark state to **1** when `dKy_darkworld_stage_check` succeeds, **2** when only `dKy_darkworld_spot_check` succeeds, otherwise **0**. State 2 therefore must not automatically enable a Twilight-only preset.
- `src/d/d_kankyo.cpp:11151`, `dKy_darkworld_stage_check`, consults stage/room eligibility and save progression. `dKy_darkworld_spot_check` at 11195 deliberately ignores completed Twilight progression. The 34-entry stage table is `src/d/d_kankyo_data.cpp:105`, `l_darkworld_tbl`.
- Palace of Twilight needs an explicit product decision, not an assumption that its name equals overworld Twilight state: weather code separately checks `D_MN08`, `D_MN08A/B/C`, and room exclusions (`src/d/d_kankyo_wether.cpp:666`). Wolf senses is likewise a separate state (`daPy_py_c::checkNowWolfPowerUp()`), with its own environment changes.

Recommended game snapshot: active Twilight equality test, stage, current room, scene validity, senses state, underwater state, and transition state if a verified accessor exists. Snapshot on the game thread; do not call live game APIs from an asynchronous render callback. Invalid/loading snapshots should disable or fade the effect. Transition interpolation and threading are proposed design, not verified existing API behavior.

## Components contributing to appearance

| Component | Verified source | Consequence for MidnaFX |
|---|---|---|
| Environment palette and fog | `src/d/d_kankyo.cpp:2283` `dScnKy_env_light_c::setLight`; fog density/color/near/far interpolation around 2418–2459; material fog assignment around 4453; `GXSetFog` at 9396/9431 | Appearance includes scene/material inputs before final composition. A final color grade cannot independently undo geometry fog. |
| Senses light/fog | Same file: `dKy_WolfPowerup_AmbCol` (124), `dKy_WolfPowerup_BgAmbCol` (315), `dKy_WolfPowerup_FogNearFar` (413) | Keep senses distinct from normal Twilight preset selection. |
| Twilight camera/player lighting | Same file: `dKy_twilight_camelight_set` (10259), invoked at 8220; it checks darkworld and excludes the `D_MN08` prefix, then configures six lights with varying color/distance/cutoff | Not reducible to bloom or saturation alone. |
| Bloom and mono/desaturation parameters | Same file: palette bloom IDs (2468), parameter interpolation (2514–2606), darkworld blur-size oscillation (2525), `dusk::ApplyBloomOverride()` (2604 vicinity) | Existing bloom tuning can change important native effects, but is parameter tuning rather than a new custom postprocess. |
| Bloom tables | `src/d/d_kankyo_data.cpp:12` `l_kydata_BloomInf_tbl`: explicitly named Twilight, Twilight Weak, Senses, Palace of Twilight records | Do not replace all records with one global value. Twilight and Palace records are already different. |
| Floating weather particles | `src/d/d_kankyo_wether.cpp:666` `wether_move_housi`: Twilight initialization commonly requests 200 particles with stage/layer exceptions and uses Always resource `0x5E`; `src/d/d_kankyo_rain.cpp:1007` `dKyr_housi_move` | Scene particles are actual draw content; a screen grade cannot identify them semantically after composition. |
| Evil/mist content | Weather `wether_move_evil` (989), packet `draw` (137); rain `dKyr_evil_draw` (6559), `dKyr_evil_draw2` (6320); environment mist colors near 11475 | Further translucent geometry/particle contributors; not evidence that all mist appears in all Twilight rooms. |
| Sky/astronomy | Weather suppresses sun/stars for darkworld at 435/597; sky actors `src/d/actor/d_a_vrbox.cpp` and `d_a_vrbox2.cpp` set material fog | Sky content/palette must be distinguished from a full-frame overlay. |
| Portal particles | `src/d/actor/d_a_kytag04.cpp:54–72` spawns particle IDs `0x84A5/0x84A6/0x84A7`; player portal search/flow is `src/d/actor/d_a_alink_demo.inc:4332` onward | Portal visuals include particles and gameplay flow. These references do not establish a unique full-screen portal filter. |

## Framebuffer composition and ordering

`src/m_Do/m_Do_graphic.cpp` contains `mDoGph_gInf_c::bloom_c::draw2` (1443), `bloom_c::draw` (1699), `drawDepth2` (1019), `retry_captue_frame` (1942), `motionBlure` (1978), and `mDoGph_Painter` (2175). PC bloom selects Dusk versus Classic mode at 1701–1705. Classic bloom uses GX framebuffer copies and TEV operations; the Dusk path has down/up-sampling and framebuffer blend stages. This is an existing native framebuffer effect, not evidence of a public shader-extension API.

Within Painter, dark opaque/translucent lists occur around 2345–2427, filter lists around 2513, screen translucent content around 2600, then a conditional framebuffer recapture for underwater/`D_MN08` around 2615 and bloom draw at 2632. The 3D-last list follows bloom. Game 2D particles and one fade path occur around 2673–2681. HUD/menu 2D drawing occurs later around 2772–2803. Thus a candidate insertion immediately after native bloom is before ordinary HUD, but is **not** the final world-complete boundary: 3D-last and some game particles still follow. A render-system hook needs an explicit inclusion contract, not just a convenient function address.

`darwFilter` (430) and `calcFade` (477), as well as dark/filter lists, are additional composition mechanisms. Their mere existence does not prove a particular visible Twilight cast originates in them. Capture/replay or controlled disabling is required to assign visual causality.

Transitions also include game time: `src/d/d_kankyo.cpp:8328` saves pre-Twilight time into `old_time` and restores it when leaving, subject to `next_time_set`. Palette interpolation uses `pat_ratio`; bloom includes temporal oscillation. Do not freeze or overwrite those game values just to switch MidnaFX presets.

## Parameter-modification versus custom postprocess

The existing developer bloom panel (`src/dusk/imgui/ImGuiBloomWindow.cpp:57`, `ApplyBloomOverride`) writes enable, mode, threshold, blur size/ratio, blend color and mono color. This supplies a useful diagnostic control and demonstrates parameter override timing. It does not provide custom shader submission, own textures, or a separately ordered composition pass.

Preferred scope separation: preserve native world/fog/particle rendering; expose optional native bloom tuning only if separately labeled; implement MidnaFX color transform/vignette/grain in a real render pass with explicit input/output, color space, resize and disabled passthrough contracts. A parameter-only prototype cannot establish the custom-pass feasibility gate. Runtime A/B should compare unchanged native rendering, native parameter-only tuning, and the custom pass, using the same camera/save/time and bloom mode.

## Dawnlight compatibility evidence and limits

The [public repository](https://github.com/BeZide93/dusk) identifies Dawnlight as a mod package and currently exposes `mod-dawnlight-main`. Local `mods/dawnlight/src/mod.cpp:44–71` installs config/UI/save/aim/item/manual-shield/new-save/jump/enemy hooks. A search of the package for bloom, fog, shader and postprocess did not find a rendering-grade implementation; this is bounded source evidence, not proof of absence across every historical branch.

`mods/dawnlight/HOOKS.md` and `NEWHOOKS.md` describe a clean-host hook package, but their deferred-feature statements lag the current source: `mod.cpp` already calls `register_new_save_modes`, and `src/new_save_modes.cpp:2379` registers the Garden of Twilight event. Prefer actual registration code and pinned revisions over those prose documents. Do not copy the fork wholesale or infer compatibility solely from its README.

MidnaFX should avoid replacing player/camera/HUD functions for render-only work. Test Dawnlight enabled and disabled, including its aiming camera and HUD layouts. Shared configuration names, direct-hook ownership and shutdown order remain runtime/API audit items. No co-loading or executable compatibility test has been performed.

## Required runtime follow-up

Verify active and cleared Faron/Eldin/Lanayru scenes; Twilight entry/exit and save reload; human/wolf senses; Palace entrance/interior/boss; underwater; portal use; fades; menu/HUD; Classic/Dusk/off bloom; Dawnlight camera/HUD modes. Record exact game/build/mod revisions, resolution, selected native bloom, stable capture setup and GPU pass ordering. The present research identifies inputs and candidate boundaries but cannot honestly claim a visual improvement, complete portal-effect attribution, or a proven safe custom-pass insertion point.
