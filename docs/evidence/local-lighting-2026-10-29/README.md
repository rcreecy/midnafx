# Native local-light response: 2026-10-29

## Product and artifact scope

Version 1.17 adds a visible scene-lighting capability driven by supported native
ordinary lights, without new asset lists. It does not create the source or replace
native lighting. The new independent control and Remaster use 15% strength;
Vanilla disables it. The [feature contract](../../local-lighting.md) records
coverage, resource costs and exclusions.

The final local Release package SHA-256 is
`6E67FBCE102BA008FBE78F98AC3233B6AC0FBBB4C45BB5C53E5B90A763AADB97`.
The initial native package, before help-placement correction, is
`EFF9A814268212229E020CA6E0BBC613EBF87A6059E3FF4C92A293FBCAB43257`.
The lighting implementation and controls are identical in both. The exact-name
Windows host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The [session receipt](native-final/session.json) and
[final reload receipt](native-final/reload.json) retain those identities.
The temporary fixture SHA-256 is
`2FCE57E0A914690345B2F6F648D54A9860CA4BA5D7A70355CEDCD74633D5E7C5`.
It is separate from the product and is not included in release packages.

## Native source anchors

These anchors refer to the pinned Dusklight source revision
`edf42c6a7202647b56dd2fcdef02d17671bc814b`:

- `include/d/d_kankyo.h:17`: `LIGHT_INFLUENCE` contains world position, signed
  `GXColorS10`, influence power/radius, fluctuation and registration index.
- `include/d/d_kankyo.h:245`: ordinary registry has 100 sparse pointer slots.
  `src/d/d_kankyo.cpp:9104` registers actor-owned pointers and `:9167` removes
  them. MidnaFX copies current values on the game thread; queued callbacks retain
  no native source pointers.
- `src/d/d_kankyo.cpp:1086`: stage power becomes a radius in native world units
  by multiplying by 200. At `:3472`, ordinary native response uses a clamped
  distance/radius falloff, rather than interpreting power as physical watts.
- `src/d/actor/d_a_obj_lv1Candle00.cpp:109` and
  `src/d/actor/d_a_obj_fireWood.cpp:91`: typical native torch/campfire sources
  use RGB 188/102/66 and radius 500. Their native set/cut lifecycle remains owned
  by the game.
- `src/d/actor/d_a_alink.cpp:14980` routes the lantern through
  `dKy_WolfEyeLight_set`, outside the ordinary registry. Boss/lantern spotlight
  attenuation and per-update activity at `src/d/d_kankyo.cpp:10164` and `:4710`
  cannot be treated as an ordinary radius. This slice excludes those sources.

Priority/special indices, negative color, invalid data and radii beyond 6,000 are
excluded. Selection ranks at most four positive ordinary sources by bounded
camera coverage and brightest color channel; ties preserve registry order.
Effect-light sources and native per-object fluctuation/material exceptions are
not reproduced.

## Automated evidence

The Release ALL build and [four focused checks](focused-final.log) pass:
`local_lights`, `presets_roundtrip`, `shader_compile` and `package_contract`.
The portable selector checks proper rigid view transforms, non-finite and
out-of-range data, signed/raw color conversion, stable top-four ranking, radius/
index exclusions and cleared output without retained history. Preset checks
round-trip every scene mask from -1 through 2047, reject 2048, and verify that
old 1023 masks retain local lighting off.

The [GPU log](gpu-final.log) contains 79 passing cases on Intel UHD Graphics 630,
D3D12. All 60 prior contact-lighting, AO/shadow, bounce, fog and sunlight checks
remain green. Nineteen new assertions establish:

- At 15%, localized colored response changes 608 receivers at 96×80 and 241 at
  odd 65×49. Corners outside the source radius retain exact color. Swapping source
  hue changes the expected receiver channel, with alpha preserved.
- Missing, disabled, back-facing and out-of-radius sources retain exact source
  color. Four overlapping lights remain within one independent linear-energy/
  highlight-headroom bound. Matched reversed-Z projections agree within one code.
- A visible opaque caster reduces illumination at 60 and five unchanged receivers
  in the two target sizes. A close-source test retains raw illumination when a
  visible caster lies beyond the source. Sources inside the normal-bias distance
  contribute nothing.
- Local and bounce contributions share one maximum gain. Zero local strength
  preserves prior bounce color and fields exactly even with poisoned source
  positions. Pure black/white endpoints remain exact; tiny 1×1 and odd 3×5 targets
  stay finite.

Review identified the first visibility implementation's missing biased-segment
bound. The final implementation rejects an origin past a nearby source and only
accepts blockers before the source, with finite depth/ray tolerances. The final
tests cover that bound; no old-build GPU failure is claimed for it. Initial
compilation also exceeded MSVC's individual string-literal limit; splitting the
common/compute strings into adjacent literals preserves WGSL bytes. A stale
preset invalid-mask fixture was changed from 1024 to the new invalid value 2048.

## Native observation

The [launcher](launch-native.ps1) uses isolated configuration, Ordon
`F_SP103,0,27,0`, D3D12 debug logging, no replacement pack and no gameplay input.
AO, contact shadows, bounce, grading and other scene effects are off; motion
clarity remains on. The [audit source](driver.cpp), with
[build definition](driver-CMakeLists.txt), registers one temporary ordinary light
after the player is ready. It uses native torch color/radius near the player and
unregisters only its own pointer during shutdown. This is a controlled registry
integration test, not a naturally placed torch scene or a product-created light.

The [native log](native-final/native.log) reports source index 2 at world
(1804.1,237.6,1247.8), RGB 188/102/66 and radius 500. Enabling Local light response
independently produces one supported source at 15%. The product reloads the final
help revision and reports one source again, while the audit source remains
registered. Final UI disable and re-enable work; the [correct help](control-final.jpg)
states the lantern exclusion and the [disabled control](control-disabled.jpg)
shows independent Off state.

The [initial control](control-initial.jpg) and [initial on scene](native-on-initial.jpg)
precede the help correction. [Off](native-off.jpg) and
[final on](native-on-final.jpg) screenshots use the same exploration camera but
unfrozen simulation and JPEG capture. The fixture already affects native actor
lighting in both states. Actor animation, particles and environment may differ;
these images are context, not an isolated pixel-quality comparison. The GPU
fixtures establish the specific colored-light and visibility behavior.

Shutdown reports 4,290 balanced compute/draw callback pairs before reload and
2,913 afterward, with one retained target set each time. Those counts do not
measure GPU completion, duration or FPS. The fixture retires its registry source,
normal close exits zero and all mods unload. The empty card emits
`Failed to open file: gczelda2`; no new shader validation failure is reported.
The [assertion script](analyze.py) generates the [summary](summary.json) from the
retained observations and final configuration.

## Remaining limits

Natural torch/campfire coverage, scene transitions, lantern/boss lights, material
semantics, whole-game motion/art quality, GPU duration and 4K performance remain
unqualified. Eight visibility taps can miss thin blockers; off-screen or hidden
geometry is unavailable. Four-source selection can change during camera movement.
The effect supplements an already lit LDR scene and can overlap native lighting.
macOS remains best effort through native builds and portable checks. Native Mac
gameplay and Metal execution are unverified; no hosted Metal runtime gate is used.
