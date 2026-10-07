# M12 water correctness review

Initial review: 2026-10-04  
Updated: 2026-10-06

Scope: the default-off exact-allowlist water pipeline, from classification and
optical-thickness capture through the currently implemented product effects.

## Findings resolved

* Removed an early actor-level duplicate-render design. It could redraw unrelated
  shapes, effects, or actor state. The retained hook targets an exact classified
  shape only.
* Corrected attachment sizing from logical GX dimensions (608x448) to the actual
  resolved scene target (1216x896), preventing depth/color extent mismatch.
* Restored the native replay setup after the diagnostic draw: material load,
  packet preparation, shape pre-draw state, and packet display list are applied
  before the original shape draw.
* Borrowed scene-depth views are valid for one frame only and are cleared at the
  next scene boundary. Private attachments are recreated on resize and released
  on disable/shutdown.
* Missing services, targets, depth, matrices, unsupported formats, and failed
  allocations bypass the diagnostic. The shader rejects background depth,
  non-finite reconstruction, and opaque samples in front of the water surface.
* Fixed visible grayscale/native-color flicker on stage water. Classification was
  refreshed only on simulation ticks but cleared on every interpolated
  presentation. Capture modes now rescan active stage actors at the pre-water
  render stage on every presentation frame; classification-only mode retains its
  simulation-tick scan.
* Prevented current/projected overlay shapes from owning optical thickness.
  Explicit actor paths select material zero only on primary/surface models;
  validated stage paths select their exact base-water material. Target identity
  is logged once for audit.
* Rejected opaque depth at or in front of the captured water plane. This removes
  banks, actors, and other non-water foreground from the diagnostic instead of
  displaying them as thickness or an error color.
* Split malformed depth reconstruction from normal occlusion in the diagnostic:
  invalid reconstruction remains red, while expected foreground occlusion is
  black and fails closed.

## Remaining risks

* Post-native auxiliary replay is visually proven on generic moving water,
  Fishing Pond, and the visible Lake Hylia patch. The shape hook still cannot
  invoke the enclosing material packet's complete draw call, so untested water
  classes could depend on additional authored state.
* Exact packets are retained only until the same frame's pre-HUD stage, then
  replayed into one combined mask/depth pass and cleared. Collection is capped at
  64 unique packets; overflow fails the frame closed. Lakebed room 3 now proves
  visible four-packet surface and submerged output.
* Lakebed room 3 now has visible surface and submerged flycam proof, including
  repeated surface crossings. A gameplay-controlled dive and additional
  story-layer states remain separate product-validation work.

## Verification

* Windows Release package builds successfully.
* All 14 tests pass, including shader compilation and package contract checks.
* Source-matched D3D12 runtime reaches mask, surface-depth, and thickness passes at
  Fishing Pond and shuts down cleanly without WebGPU/D3D12 validation errors.
* A 347-capture run reported zero capture failures and about 217 microseconds of
  CPU hook wall time per captured frame; GPU duration is not yet measured.
* Lake Hylia, generic moving/swimmable water, and Lakebed Temple each completed
  259-260 captures with zero failures, clean unload, and 94-100 microseconds of
  CPU hook wall time per capture. These runs prove execution, not pixels.
* Fishing Pond visual proof shows a stable grayscale thickness field with live
  HUD after the cadence fix. A 10,006-capture run had zero failures, no visible
  flicker, clean unload, about 24 microseconds of classification scan time, and
  about 139 microseconds of capture/replay CPU wall time per frame.
* Generic moving water selected the exact `cc_MA06_nigori_v_x` base surface after
  an overlay-selection failure was diagnosed. The corrected view contains a
  continuous grayscale field with Link and stone banks rejected by foreground
  depth, no red/magenta reconstruction faults, 6,471 successful captures, zero
  failures, and clean unload.
* Lake Hylia produced a stable water-local grayscale field without reconstruction
  fault colors, then unloaded cleanly after 688 successful captures and zero
  failures.
* Gate 3 passes for the validated exact path. Multi-packet aggregation is bounded
  and implemented, but broader material coverage remains outside this gate and
  constrains the first product allowlist.

## Absorption prototype review

* The new setting is default off and persisted through the existing settings
  migration path. It does not mutate gameplay water, geometry, or native draw
  state.
* The shader rejects missing masks, background depth, invalid reconstruction,
  and foreground occlusion. Inputs are frame-scoped and resize-safe through the
  existing water attachment lifecycle.
* Visual review found that normal grade selection overwrote the absorption
  pipeline kind. The active log was therefore misleading. The selection guard
  now preserves `WaterAbsorption` and `WaterAbsorptionDetail`.
* A zero-strength control then proved that the original capture order replaced
  native water with an incomplete replay. Capture now leaves the original draw
  untouched and performs the white mask/depth replay in a private pass afterward.
* R_SP107 zero-strength A/B preserves native transparency and animated
  distortion. Default strength produces a restrained depth tint without losing
  those details. The final D3D12 run completed 1,449 captures with zero failures,
  clean unload, about 88 microseconds of CPU hook time per frame, and no WebGPU
  validation errors.
* Fishing Pond zero/default A/B preserves waves, transparency, shoreline, and
  background geometry while the default pass adds a restrained depth tint. The
  runs completed 1,171/1,143 captures with zero failures at about 138/147
  microseconds per frame and unloaded cleanly without validation errors.
* Lake Hylia zero/default A/B remained stable on the small visible water patch
  and retained native detail. The runs completed 1,334/1,038 captures with zero
  failures at about 86/85 microseconds per frame and unloaded cleanly.
* Lakebed Temple exercised four exact surface packets per frame. It completed
  1,464 combined captures with zero failures at about 52 microseconds of CPU
  replay work per frame, clean unload, and no validation errors. The spawn does
  not expose water, so this proves execution/lifecycle rather than pixels.
* A later all-features Lakebed room-3 run completed 11,900 captures with zero
  failures, four packets per frame, `light=yes`, and clean unload, but measured
  about 1.61 milliseconds of CPU replay work per frame. The large variance from
  the earlier run was reproduced at about 1.437 milliseconds with specular off,
  localizing it to the surface replay rather than the optical shader.
* Replay review found two redundant operations: an explicit `prepareDraw()`
  immediately before the original `J3DShapePacket::drawFast()` performed the
  same preparation again, and `material->load()` rebound source textures/NBT
  state that the untextured mask does not consume. The material reload also
  produced two stale-table texture-index errors per packet per frame.
* Removing those calls reduced the same four-packet Lakebed path to 198,970
  microseconds over 3,678 captures, about 54 microseconds per frame. The run had
  zero failures, zero texture-index errors, normal rendering, and clean unload.
  A visible R_SP107 surface-swimming repeat retained the enhanced water result
  and measured 42,400 microseconds over 1,002 one-packet captures, about 42
  microseconds per frame, with zero failures and clean unload. Evidence is
  `optimized-swim-enhanced.jpg` under `build/m12-water-surface/`.
* R_SP107 room-1 matched native/enhanced surface-swimming captures retained
  Link, authored surface motion/transparency, HUD, and surrounding geometry.
  The enhanced run completed 392,535 one-packet captures with zero failures at
  about 251 microseconds per frame and clean unload; a shorter repeat completed
  3,134 captures with zero failures at about 219 microseconds per frame.
  Evidence is `underwater-surface-native.png` and
  `underwater-surface-enhanced.png` under `build/m12-water-surface/`.
* This proves stable swimming at the air/water boundary.
* Lakebed room-3 flycam validation exposed real dungeon water pixels and crossed
  the surface repeatedly. Enhanced water retained authored surface detail,
  submerged fish, underwater distortion, surrounding geometry, and the surface
  boundary. Native/enhanced evidence is `dungeon-native.jpg`,
  `dungeon-enhanced.jpg`, `dungeon-underwater-native.jpg`, and
  `dungeon-underwater-enhanced.jpg` under `build/m12-water-surface/`.
* The visible enhanced run completed 17,205 captures with zero failures and
  four packets per frame at about 53 microseconds of CPU replay work per frame.
  The dedicated submerged transition run completed 9,361 captures with zero
  failures at about 53 microseconds per frame. Both unloaded cleanly with no
  texture-index or WebGPU/D3D12 validation errors. This is a camera-state proof;
  a gameplay-controlled dive remains a separate input-path check.

## Animated surface-normal review

* Two procedural layers use different world-space scales and opposing scroll
  directions. Validated camera matrices and captured surface depth anchor the
  pattern; invalid water/background depth bypasses the full water effect before
  reconstruction.
* Wave strength is persisted, bounded to 0-100%, and only participates while the
  default-off enhanced-water path is active. Zero disables the added variation.
* Fishing Pond passed 0%, 100%, and default-20% runs. The effect remained local
  to water, retained native animated texture and transparency, and showed no
  validation errors. The default run completed 1,289 captures with zero failures
  and clean unload.
* CPU replay work remained about 51 microseconds per frame with two packets. GPU
  cost of the added trigonometric shader work remains unmeasured.

## Refraction prototype review

* The opaque scene-color resolve is requested together with the existing
  pre-water depth resolve only when enhanced water is enabled. Both views remain
  frame-scoped and are consumed before the after-HUD cleanup hook.
* Refraction is bounded to eight pixels and 35% blending. Sampling clamps to the
  viewport and rejects background depth plus opaque samples at or in front of
  the water surface. Missing color/depth/mask inputs fail the effect closed.
* Fishing Pond passed 0%, default-15%, and 100% runs. Maximum strength exposed
  the intended water-local distortion without visible shoreline, Link, HUD, or
  unrelated-translucency contamination. No recursive water image, gross edge
  smear, validation error, or shutdown failure was observed.
* The maximum run completed 3,164 captures with zero failures, two packets per
  frame, about 45 microseconds of CPU replay work per frame, and clean unload.
  The later Lakebed surface/submerged flycam passes exercised the combined path
  without recursive image, gross edge smear, or transition failure. GPU shader
  cost remains unmeasured.

## Fresnel fallback review

* Schlick Fresnel uses reconstructed view position and the shared animated
  surface normal. Zero-length and NaN vector lengths return the transmitted
  result, and the final reflection blend is capped at 45%.
* Strength and RGB tint controls are persisted and bounded. The feature is
  reachable only through the default-off enhanced-water path and exact surface
  mask; unavailable camera/depth inputs already fail that path closed.
* A fresh Fishing Pond run at 100% strength kept near-normal water transmissive
  and added the expected cool grazing response without visible contamination of
  Link, shore terrain, HUD, or unrelated translucency. The 3,123-capture run had
  zero failures, about 44 microseconds of CPU replay work per frame, clean
  unload, and no validation errors.
* This is a tint fallback, not a claim of scene-accurate reflection. A semantic
  environment color remains unavailable and SSR remains deferred.

## Shoreline treatment review

* Shoreline weight is derived only after exact mask, valid surface/opaque depth,
  and foreground-occlusion checks pass. It uses normalized optical thickness,
  clamps the active band to the shallowest 8%, and caps color blending at 16%.
* Strength is persisted and bounded from 0-100%. Zero returns the prior result;
  default is 20%. The shader does not alter alpha, water geometry, gameplay,
  collision, or TP's authored interaction effects.
* Fishing Pond passed 0% and 100% runs. The maximum remained restrained, avoided
  hard halos and bright foam, and did not contaminate Link, shore terrain, HUD,
  or unrelated translucency. Maximum/zero completed 4,702/4,857 captures with
  zero failures, clean unload, no validation errors, and about 51/56
  microseconds of CPU replay work per frame.
* Lakebed dungeon edges and repeated underwater transitions passed live flycam
  validation.
* Lake Hylia `F_SP115`, room 0, start point 20 passed an irregular natural-shore
  comparison after the 15% absorption correction. The curved rock boundary,
  platform, Link, HUD, and environmental particles remained unaffected. The
  run completed 3,180 captures with zero failures, one packet per frame, about
  40.0 microseconds of CPU replay work per capture, clean unload, and no
  WebGPU/D3D12 validation errors.

## Environment-aware specular review

* The light source is TP's authored `g_env_light.base_light.mPosition`.
  `SetBaseLight()` selects sun, moon, or stage light using native environment
  rules. MidnaFX reads this state without changing game interpolation or light
  ownership.
* Missing and non-finite light positions disable specular for the frame. Shader
  guards reject zero-length and NaN view, normal, light, and half vectors.
  Strength is persisted, bounded from 0-100%, defaults to 12%, and only reaches
  the exact-mask, default-off enhanced-water path.
* Fishing Pond passed matched 0% and 100% runs with `light=yes`. Maximum remained
  water-local and did not visibly affect Link, shore terrain, HUD, or unrelated
  translucency. Maximum/zero completed 7,199/6,211 captures with zero failures,
  clean unload, no validation errors, and about 50/56 microseconds of CPU replay
  work per frame.
* A controlled Fishing Pond run exercised noon (`180`), night (`330`), and the
  native sun/moon selection boundary (`67` then `68`). Authored scene lighting
  changed without exploding, inverted, or blown-out water specular, NaN/Inf
  state, or WebGPU/D3D12 validation errors. The run completed 16,478 captures
  with zero failures, about 54 microseconds of CPU replay work per capture, and
  clean unload. Evidence is `specular-fishing-noon.jpg`,
  `specular-fishing-night.jpg`, `specular-fishing-time067.jpg`, and
  `specular-fishing-time068.jpg` under `build/m12-water-surface/`.
* Lakebed execution reached the environment-aware path with `light=yes`; the
  later flycam pass exposed water pixels above and below the surface without
  unstable, inverted, or blown-out response.
* A matched Lakebed room-3 flycam run held the underwater camera fixed while the
  live console changed TP time from night (`330`) to noon (`180`). The central
  world crop increased from 49.55 to 54.62 mean luma (about 10.2%) while water,
  submerged geometry, fish, and distortion remained stable. It completed
  1,399,500 captures with zero failures, four packets per frame, about 47.5
  microseconds of CPU capture/replay work per frame, clean unload, and no
  texture-index or WebGPU/D3D12 validation errors. Evidence is
  `dungeon-light-night.jpg` and `dungeon-light-noon.jpg` under
  `build/m12-water-surface/`. The exact `base_light` vector was not logged, so
  this proves stability across an authored dungeon state change without claiming
  a measured direction change.
* GPU shader duration remains unavailable through the public source-matched host:
  `GfxService` has no timing-result contract, Aurora timestamp collection is
  compiled only with `TRACY_ENABLE`, and this runtime does not expose
  `TimestampQuery`. CPU hook measurements are not GPU timings. A useful host
  extension would expose optional asynchronous per-pass timing without a render-
  path wait or readback. Broader enablement remains blocked on additional
  exact-class coverage, not on inventing a private timing hack.

## Optional SSR research review

* An eight-step prototype used only existing frame-scoped scene color/depth and
  camera matrices. It bounded loop cost, rejected invalid/off-screen rays and
  background depth, and retained the environment fallback on misses.
* A magenta-hit/black-miss diagnostic at Fishing Pond produced no magenta water
  pixels. The radial-distance crossing rule did not establish one valid hit.
  Runtime remained stable for 7,750 captures with zero failures and clean
  unload, but stability does not satisfy SSR correctness.
* The prototype was reverted completely. No SSR control or runtime shader path
  remains. A second experiment used signed view-space depth and a miss-reason
  view. Both normal and widened bounded traces produced only no-crossing and
  off-screen results, with no valid hit. SSR needs a stronger intersection
  method and a high-contrast validation scene before returning to product scope.

## Submerged-depth discontinuity review

* A matched Fishing Pond comparison found that full-strength absorption/scatter
  exposed coarse submerged triangles as large hard-edged brown silhouettes.
  Native water at the same time, FOV, eye, and center did not show them.
* The correction preserves TP's authored transmitted surface and caps the
  Beer-Lambert/scatter contribution at 15%. It does not weaken the separate
  refraction, wave-normal, Fresnel, shoreline, or specular controls.
* A 45% intermediate remained visibly blocky and was rejected. The final 15%
  run removed the objectionable silhouettes at the matched view while retaining
  restrained optical-depth variation.
* The final run completed 8,919 captures with zero failures, two packets per
  frame, about 47.8 microseconds of CPU capture/replay work per capture, clean
  unload, and no WebGPU/D3D12 validation errors. The adjacent waterfall stayed
  outside the exact water-surface allowlist.

## Broad world allowlist review

* Fixed a correctness risk in the original stage-name-only checks. Static water
  now requires an exact stage, runtime room, background role, and material name.
  A same-named material in another room or model role is rejected.
* The pure identity component contains 110 entries across 26 stages and 43 rooms.
  Unit coverage proves accepted primary and companion identities plus rejection of
  wrong-room, waterfall, sunbeam, oil, and unknown-stage inputs.
* Runtime logging now records actor room and stay room. The desert-lake probe
  confirmed `F_SP118` room 2 reports actor room 2 and stay room 2, matching the
  archive identity rather than depending on a guessed global room.
* The live confidence set covered visible outdoor water in `F_SP103` and `F_SP108`,
  visible under-bridge water in `F_SP122`, exact Lakebed static layers in `D_MN01`,
  the large-water room in `F_SP117`, and the special desert-lake model in `F_SP118`.
  No unrelated opaque or translucent geometry was marked in the visible checks.
* `F_SP115` room 0 includes runtime-only background-object identities absent from
  the static room-model scan. Its primary MA06 layer remains the only added
  thickness candidate; MA09 and indirect companion layers are classified without
  causing duplicate thickness replay.
* The stage scan remains bounded to stages present in the table and measured about
  19.5-52.0 microseconds per scanned frame in the drawing scenes. The table lookup
  is read-only, allocates nothing, and unknown identities fail closed.
* Full enhanced rendering then passed on newly covered `F_SP108` and `F_SP122`
  surfaces. The runs completed 578 and 326 captures, respectively, with zero
  failures, exactly two packets per frame, clean unload, no validation errors, and
  no visible contamination of actors, shore vegetation, bridge geometry, dialogue,
  or HUD.
* Waterfalls/cascades, fountains, sunbeams, oil, aquarium glass, debug materials,
  particles, and spray remain excluded. Treating those as flat refractive water
  would be a feature and correctness expansion, not allowlist completion.
* Windows release build, shader validation, package contract, and all 15 tests pass
  with the classification diagnostic restored to default off.

## Dynamic/story-water confidence follow-up

* `F_SP109,0,14,0` visibly isolated the hot-spring pool. Enhanced rendering kept
  authored ripple/foam detail and nearby mist while completing 594 captures with
  zero failures at about 43 microseconds of CPU capture/replay work per frame.
* `D_MN01A,50,0,0` exercised `lakebed-boss` directly. Magenta covered only the
  arena surface, and enhanced water completed 668 captures with zero failures at
  about 45 microseconds per frame. A native control matched the arena's dark base
  exposure, ruling out an enhancement-caused blackout.
* Both enhanced runs unloaded cleanly. No WebGPU/D3D12 validation, NaN, or Inf
  fault was observed.
* A direct room-3 Lakebed warp left rotating-stair water switch-gated. A removed
  developer-only activation established its live actor path and exact three
  materials, but did not expose them in the camera. No story-state pixel proof is
  claimed.
* Enhanced water now defaults on for new installations. The exact broad allowlist
  remains fail-closed and saved user choices remain authoritative. Story-gated
  rotating water and transition coverage remain release-qualification gates.
