# Pre-v1 product-quality review — 2026-10-03

## Disposition

MidnaFX is **not yet a fully validated, polished v1 product**. This pass replaces
the exact-only geometry gate with conservative automatic qualification, adds
product profiles and a clearer settings hierarchy, and records fresh Windows
evidence. Useful automatic coverage, skinned coverage, live profile interaction,
and the requested visual/lifecycle matrix remain incomplete. Defaults stay
conservative. No new water, atmosphere, AA, camera, or DOF rendering work was added.

## Repository and release audit

- Starting HEAD: `69aedbfd44bd9d3511d0df5985c842df9b3bc540`, branch
  `codex/m12-water`, initially clean. Divergence was reported before edits.
- Expected README commit `3f61159c35fd1ecd8ba3f87e39bf7bbf8400121c` is on main,
  not an ancestor of the starting checkout. Common ancestor is release commit
  `bc6fcbd4f6aae3579afef234a3deee77177fb8e5`. Three intervening water commits
  were already present; this session does not extend them.
- [v1.0.0](https://github.com/rcreecy/midnafx/releases/tag/v1.0.0) is published,
  not draft or prerelease, with Windows and Intel Mac packages. Release timestamp
  is 2026-10-03 19:12:41 UTC. It predates these changes.
- [Main CI](https://github.com/rcreecy/midnafx/actions/runs/37148339952) and
  [release CI](https://github.com/rcreecy/midnafx/actions/runs/37146976446)
  succeeded. New commits have local checks only; no new remote CI or release claim.
- Reviewed README, architecture/development, geometry/M8 and index-splitting
  investigations, camera/DOF investigations, milestone reviews, v1 readiness and
  release correctness, build workflow, CMake presets, and deterministic tests.
- Pinned host/SDK: `edf42c6a7202647b56dd2fcdef02d17671bc814b`, with the five
  existing CameraService host-patch files modified locally. CI pins the same
  revision and applies the camera patch on Windows and macOS Intel runners.
- A concurrent task began editing water/renderer/settings. The user authorized
  pausing it. Its four dirty files remain outside this session's commits. The
  native build includes its missing animation-header fix and scene-capture code,
  with both water diagnostics disabled. This is a mixed working-tree build,
  **not proof of a clean checkout build at the final commit**. The starting
  checkout's water compile failure is not repaired by these product commits.

## Geometry architecture and acceptance

See [automatic-smoothing.md](automatic-smoothing.md) for interception, decoding,
ownership, fingerprints, and classifier rules. Runtime qualification accepts
compatible single-joint/single-draw-matrix rigid models without a fingerprint.
Exact pot, Link, nest, pumpkin, and rock fixtures retain their validated paths.
They do not bypass bounds, finite input, encoding, normal conflicts, or ambiguous
orientation. A known-bad veto exists in the classifier and is tested; no asset
blacklist has been populated without evidence. Unknown skinned models remain
unsupported. No generic normal-index rewrite or position welding was added.

- **SAFE:** supported arrays, valid reconstruction and indices, finite input,
  compatible ownership, unambiguous adjacency and smoothing groups, no unresolved
  normal-index conflicts. SAFE with no changed normals performs no mutation.
- **AMBIGUOUS:** degenerate/nontriangle input, coincident identities, non-manifold
  edges, disconnected fans, inconsistent winding, overlapping angle groups,
  uncertain face orientation, or incompatible shared normal indices.
- **UNSUPPORTED:** malformed decode/layout, unsupported NBT or normal encoding,
  unknown matrix/skinning ownership, missing restore hook, shared normal storage,
  or work bounds. Both rejected classes leave normal bytes untouched.

Material boundaries and authored normal discontinuities remain mandatory. The
default face angle is 55 degrees, authored split threshold 20 degrees, and
geometric contribution 25%. Automatic compatibility must form disjoint groups;
non-transitive A-B/B-C compatibility cannot choose an order-dependent group.
Classification runs once on resource load, with results cached until archive
deletion. No per-frame topology analysis was introduced. Cache cap: 4,096 models;
mutation cap: 1,024 arrays and 16 MiB of retained normal backups.

## Corpus and Windows evidence

Machine: Intel Core i7-10700, Intel UHD Graphics 630, driver 31.0.101.2140;
Windows reports version 10.0 build 26200. Source-matched RelWithDebInfo Dusklight
reports `UNKNOWN-VERSION` plus the pinned SHA. Backend D3D12, scene resolution
1216×896. Isolated mods contain MidnaFX only: Dawnlight absent, no texture pack.
Grading, detail, FOV/latitude camera, target autofocus DOF, and both shading
classes are enabled for the scene smoke runs. Water diagnostics are disabled.

| Scene | Models | SAFE / AMBIGUOUS / UNSUPPORTED | Applied / automatic | Restored |
|---|---:|---:|---:|---:|
| Ordon `F_SP103,0,27,0` | 134 | 10 / 80 / 44 | 3 / 1 | 3, byte-exact |
| Forest `D_MN05,19,0,-1` | 90 | 13 / 46 / 31 | 4 / 2 | 4, restore logs |
| Dungeon `D_MN04,7,0,-1` | 107 | 14 / 57 / 36 | 2 / 2 | 2, restore logs |

Deduplicated by archive/file: **196 runtime models**, 23 SAFE, 115 AMBIGUOUS,
58 UNSUPPORTED. Of 192 identities outside known-good fixtures, 19 automatically
qualify (9.9%), but only **3 automatically change normals (1.6%)**. This is not
broad useful coverage. Identity deduplication is not a byte-fingerprint census.

New automatic mutations: `Always/breakwoodbox.bmd` (32 triangles/18 positions/80
normals), `OBJ_ITO/k_kumo_ito00.bmd` (20/16/42), and `@bg0016/model1.bmd`
(300/173/794). Known-good mutations: pumpkin, nest, rock, pot. Link/Bmdl, Midna,
enemies and other multijoint models are present but fail closed; this is rejection
coverage, not proof of smoothing animated characters. The historical Kmdl Link
GPU-skinning proof is not a new classifier run. A second skinned character is
still unvalidated.

The [runtime CSV](validation/product-runtime-corpus.csv) records every scene/model,
classification, reason, counts, conflicts, timings, tracked memory and applied
status. It explicitly marks absent individual visual comparisons. Hard-surface,
organic props, stage geometry, thin geometry, multiple materials, shared arrays,
and index-conflict cases occur naturally in these loads. Their presence does not
prove attractive smoothing. In particular, the new box/debris and thin-geometry
mutations need close inspection before promotion.

The [offline CSV](validation/product-offline-corpus.csv) covers 42 original BMDs
from 13 archives: props, foliage-like geometry, Link, Zelda, an enemy, outdoor
and dungeon architecture. Results: 8 SAFE, 19 AMBIGUOUS, 15 UNSUPPORTED; only two
change normals. `tools/qualify-corpus.py` runs the production decoder/classifier
through `qualification_corpus`. These fixtures use original GX data and shape ID
as a conservative material boundary; they are **not equivalent to runtime
material mapping or optimized/rebuilt topology**. No game assets are committed.

| Scene | Decode total | Classification total | Planning total | Largest model total | Max tracked vectors | Retained backups |
|---|---:|---:|---:|---:|---:|---:|
| Ordon | 16.582 ms | 6.321 ms | 0.767 ms | 6.333 ms | 1,961,984 B | 13,788 B |
| Forest | 11.538 ms | 9.610 ms | 0.356 ms | 4.398 ms | 1,479,532 B | 7,194 B |
| Dungeon | 14.319 ms | 10.906 ms | 0.990 ms | 7.084 ms | 2,216,980 B | 5,244 B |

These are individual CPU samples, not averages or GPU timings. Decode includes
resource hashing and runtime extraction. Vector capacity excludes allocator,
stack, process overhead and some early-rejection temporaries in older runs;
**process peak temporary memory was not measured**. Ordon uses final corrected
separate planning-time instrumentation; CSV classification/plan times come from
the dedicated status record in all runs. Other scene instrumentation predates
the final rejection-memory correction. Rejected resources avoid repeat analysis
through the cache; their initial full decode is still part of load cost.

Final Ordon mutation costs: box 81 us, pumpkin 674 us, nest 179 us. Shutdown
compares restored destination bytes against backups: 480, 10,908 and 2,400 bytes
all report `exact=yes`, then `all mods unloaded`. Forest and dungeon counts also
match applied arrays, but those runs predate byte-comparison logging. None of
these runs proves in-process archive reload, mod reload, disable/re-enable, or
all shared-instance cases with the new classifier. Existing lifecycle source
paths remain intact; historical tests do not replace rerunning that matrix.

Local logs and scripts remain under `build/product-runtime` and `build/`.
The isolated empty memory card logs `Failed to open file: gczelda2`; gameplay
still starts. Teardown can report aborted buffer mapping/device destruction after
mod unload. No MidnaFX GPU validation error was observed in these runs.

## UX and persistence

Basic exposes grading, Enhanced model shading, modern exploration camera, DOF,
and profiles. Advanced contains grading/detail/Twilight/model/camera/DOF tuning.
Developer contains image/depth/shader/timing and model qualification diagnostics.
Expert controls are retained. Sections are one host settings panel, not separate
tabs. Legacy looks, including the explicitly named shader diagnostic, remain in
the selector; this is still a product-polish limitation.

Vanilla+ uses restrained grading, subtle detail and native camera. Enhanced uses
Natural / Vivid Realism, subtle detail and modern FOV. Both explicitly leave
geometry and DOF off until validation warrants promotion. Cinematic is withheld.
Profiles capture grading/detail and six feature switches. They do not overwrite
advanced camera, DOF, geometry or Twilight tuning. Custom retains a snapshot
across profile switches; explicitly choosing a profile can disable existing
shading/DOF. Disabling shading restores bytes immediately; re-enabling needs a
resource reload.

MFX3 stores product flags; MFX1/MFX2 remain grading/detail-only. New
`profile_data` and `profile_custom_data` keys copy valid legacy values only when
empty. Legacy keys remain untouched for rollback; new profile saves are not
visible to an older binary. Twilight targets remain grading-only MFX2. Malformed
input is rejected without replacing decoded output. Config write failures log
warnings and select Custom instead of advertising a fully applied profile;
multi-key writes are not transactional. The saved Custom snapshot is retained
for recovery.

Codec tests cover all flag masks, legacy formats, malformed flags and built-in
settings. Source review found and fixed duplicate registration during migration.
The host loaded the new settings code. Live dropdown/save/restart/migration
interaction was not completed: F1 did not open the panel in this input setup.
No synthetic claim of live persistence proof is made.

## DOF art-quality pass

Local captures: `build/product-runtime/visual/dof-on.png` and
`build/product-runtime/visual-off/dof-off.png`. Same Ordon spawn and camera,
different process/idle pose and moving scene content; not a frame-matched pair.
DOF visibly blurs the background while Link's hair/back/legs and HUD remain
sharp. No obvious gross silhouette halo appears against this green background.
This supports the existing rejection logic only for this case. No shader change
was justified. DOF remains **experimental and default off**.

Bright sky, dark interior, weapons, grass/fences/foliage close-ups, particles and
translucency, near/far edge stress, combat, camera motion, focus changes, boss
arena, transitions/dialogue/cutscenes and temporal pumping remain untested in
this pass. Historical movement/resize proof is recorded separately in the DOF
investigation. The two captures do not close those art-quality gaps.

## Camera and anti-aliasing decisions

Existing CameraService 1.3 supports FOV and chase latitude before native
smoothing/collision; 1.4 supplies a semantic target for focus. Existing controls
already expose vertical composition and a 0.35-second default FOV transition.
No new camera behavior was added without experience evidence. Distance, dynamic
distance, shoulder offset, indoor compression, swimming and horseback profiles
need an explicit host ownership seam and native special-mode proof. Direct eye
or target writes are not an acceptable shortcut. Lock-on, aiming, first person,
events and boss fallback remain governed by the unchanged fail-closed checks.

Pinned Aurora has `AuroraConfig::msaa`, but `m_Do_main.cpp` does not set it;
Aurora maps zero to one sample. No supported native 2x/4x user control was found.
MidnaFX grading and DOF also require `sample_count == 1`. Thus native MSAA was
**not runtime-tested**, and merely removing those guards would be unsafe.
Aurora has color resolve and multisampled depth snapshot code, but compatibility
still needs a bounded host experiment: expose adapter-supported sample counts,
test 4x (2x only if supported), and prove snapshot/depth/pass layouts first.
Stop there before choosing an AA shader. FXAA/SMAA were not added; no ordering,
visual or resolution-cost claim is made. TAA remains excluded.

## Dedicated correctness review and remaining gates

- **Restoration/classification:** backup ownership is published before writes;
  allocation failure cannot strand a written array without a backup. Archive
  pre-delete and shutdown restore before bookkeeping release. Shared arrays
  across distinct loaded model objects reject writes. Unknown models fail closed
  unless structurally proven; unknown skinning never receives automatic writes.
- **Persistence/profiles:** legacy storage retained; bounds and masks validated;
  Custom and feature switches reviewed. Live migration/restart and injected
  service-failure tests remain open. UI diagnostic grouping still needs polish.
- **Camera ownership:** no new camera mutation. Native mode and permission guards,
  callback unregister and fallback retained; no new special-camera runtime proof.
- **DOF lifetime/resize:** no lifetime code changed. Four-stage-callback retirement retains
  old targets for queued callbacks. Pinned Aurora uses two frame slots and releases
  each only after end-frame callback execution (`gfx/frame.hpp`, `gfx/frame.cpp`);
  loader deactivation drains callbacks before mod shutdown. This source rationale
  is host-specific. Resize/reload were not rerun with this build.
- **AA ordering/lifetime:** no new AA resources or pass ordering. Existing
  single-sample guards remain intact.
- **Optional services/shutdown:** existing Config/UI/Gfx/Camera/Hook null checks
  and version gates remain. LogService is required. Graceful Windows shutdown
  passed; absent-service combinations and mod reload require fresh runtime proof.
- **Tests:** fresh portable 12/12 and Windows 14/14 pass, including classifier,
  profile codec, camera patch contract, Dawn shader compile and package contract.
  No new remote CI, Mac execution or process-memory peak measurement.

No live Intel Mac was available. No Metal, 1080p/1440p/native-resolution or
Dawnlight coexistence validation is claimed. New playthrough-style coverage is
limited to three scene loads and one Ordon DOF comparison. Day/night, active
Twilight/transitions, bright/dark interiors, combat/effects, water-view regression,
horseback/swimming, dialogue/cutscenes, inventory and in-process scene transitions
remain required. A water-view regression would test interaction only, not water
modernization.

Blocking next steps are: improve useful automatic coverage with explicit matrix
and index-ownership evidence; validate a second skinned character; compare all new
mutations visually; complete lifecycle and profile persistence interaction tests;
finish DOF/combined visual sweeps; settle AA through the bounded MSAA experiment;
obtain Intel Mac evidence or retain a clearly unvalidated platform designation;
and validate a clean product checkout independently of the paused task's edits.
Do not weaken rejection thresholds to inflate coverage. Water modernization,
SSAO, atmosphere/material replacement, subdivision/mesh replacement, photo mode,
SSR/PBR and TAA are separate post-v1 work, not fixes undertaken here.

The smallest next geometry experiment is component-level qualification: build
complete reverse references for each normal index, permit a write only when all
its corners belong to one independently qualified component, and retain every
other normal byte. This could avoid rejecting an entire rigid model because one
region is ambiguous, without duplicating indices. It needs adversarial tests and
visual proof before replacing the current whole-model rejection. For unknown
skinned models, first decode each corner's effective matrix palette/influences;
joint count alone cannot prove compatibility. The current corpus supplies the
failure cases. This session stops these branches at that evidence boundary.
