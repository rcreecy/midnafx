# M7 geometry investigation — Gate 1 controlled proof

Pinned Dusklight revision: `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
CameraService and camera overrides remain deferred. This checkpoint contains source
research, a read-only resource catalog, and a default-off, single-model normal
mutation experiment. A later controlled view of the named model proved a
rendered lighting change; smoothing remains unimplemented.

## Decision

**Gate 1's binary renderer proof passes for the named rigid metal box.** The
supplied USA GZ2E01 RVZ was inspected locally without adding game assets to the
repository. The scoped PoC produces an unmistakable lighting change in TP's
renderer, documented below with original/negated captures. Runtime lifecycle
and skinned-model checks remain open, as does Gate 2 topology validation.

## Normal ownership and ordering

1. `dRes_info_c::loadResource` obtains each archive file through
   `mArchive->getIdxResource`, then calls `loaderBasicBmd` for BMDR, BMDV, BMDE,
   BMWR, and BMWE nodes (`src/d/d_resorce.cpp:327–423`). BMDP, BMDG, BMDA, and
   debug BMDL take different loader paths, so this is a deliberately narrow hook
   candidate, not a universal model-load callback.
2. `J3DModelLoaderDataBase::load` selects the loader. `J3DModelLoader::load` saves
   the input buffer as `J3DModelData::mpRawData`, processes VTX1 with `readVertex`,
   processes SHP1 with `readShape`, and finishes the hierarchy and shape table
   (`libs/JSystem/src/J3DGraphLoader/J3DModelLoader.cpp:42–131`).
3. `readVertex` assigns `J3DVertexData::mVtxNrmArray` with an offset relative to
   the VTX1 block. It does not allocate an independent normal array. On PC,
   `readVertexData` obtains the attribute stride/count and endian-fixes the array
   **in place** (`J3DModelLoader.cpp:325–412, 531–569`). This demonstrates writable
   memory during loading for supported resources; it does not establish that an
   arbitrary stale pointer can be written after resource unload.
4. `J3DShapeFactory::newShapeDraw` builds each matrix-group draw from the SHP1
   display list. PC `J3DShapeDraw` may optimize that list and stores a copied DL;
   the normal values remain in the vertex array. `J3DShape::makeVtxArrayCmd`
   supplies the array pointer and format (`J3DShapeFactory.cpp:130–142`,
   `J3DShapeDraw.cpp:9–29, 153–173`, `J3DShape.cpp:189–285`).
5. `dRes_info_c::loaderBasicBmd` runs material work and then
   `newSharedDisplayList`, `simpleCalcMaterial`, and `makeSharedDL` before it
   returns (`src/d/d_resorce.cpp:242–325`). Those functions create material DLs;
   their inspected source does not copy normal *values* into the material DL.
   Nevertheless, a hook on the **return** of `loaderBasicBmd` is later than
   shape VCD/VAT commands and shared material DL creation. A source-level path
   to changed values exists, but actual Aurora upload/lighting still needs the PoC.
6. `J3DModel::create` stores the model-data vertex object in each instance's
   `J3DVertexBuffer`. `setVertexData` initially points current and transformed
   normals at the model-data normal array. `setArray` sends the current array
   to `j3dSys`, and `J3DShape::loadVtxArray` can load a changed array pointer
   (`J3DModel.cpp:55–110`, `J3DVertex.cpp:36–85`, `J3DShape.cpp:145–183`).
   Consequently a load-time in-place edit is expected to affect instances that
   subsequently use the shared model data, subject to skinning and renderer
   behavior that only a runtime test can confirm.
7. Skin deformation is a special case. `J3DModel::setSkinDeform` can call
   `J3DSkinDeform::transformVtxPosNrm`, which writes the **model-data** normal
   array in place for weighted envelopes (`J3DModel.cpp:390–406`,
   `J3DSkinDeform.cpp:441–477`). Later vertex buffers may copy or transform
   normals (`J3DVertex.cpp:108–137`). A first experiment should reject
   weighted/skinned models; reversing a post-skinning edit without coordinating
   that transform is not safe.
8. `J3DVertexData` and `J3DModelData` have empty destructors. The normal buffer
   is owned by the resource archive, not either J3D object. `dRes_control_c` counts
   resource references and destroys `dRes_info_c` at zero; its destructor releases
   the data heap and unmounts the archive (`src/d/d_resorce.cpp:37–49,
   855–872`). A pointer-keyed backup cannot outlive that event. A proposed
   implementation must either restore before archive teardown and mod detach or
   use an explicitly owned replacement with an equally explicit release path.
   Reused archive buffers and reloads also require a per-resource identity check.

The buffer is writable in the loader's PC fixup path. The PoC now restores the
original bytes before archive destruction or mod detach, and rejects skinned models.
Its behavior with shared instances and in actual rendering still needs validation. A bare
`loaderBasicBmd` return hook cannot identify a file by name: its arguments are only
the node tag and raw pointer. An allowlisted experiment needs a higher-level
archive/file association, verified against an actual asset, before mutation.

## Interception candidates

| Point | Ordering | Limitation |
| --- | --- | --- |
| End of `J3DModelLoader::readVertex` | After PC endian fixup, before SHP1 | Engine-wide virtual/member hook, no complete shape or resource name yet. |
| End of `J3DModelLoader::load` | After shape setup, before caller's shared material DL | Engine-wide, no archive/file identity; may include unsupported loader paths. |
| `dRes_info_c::loaderBasicBmd` return | Narrow game-side path, after complete model load | After shared material DL; no file name; misses other BMD node types. |

The implemented PoC instead hooks `dRes_info_c::loadResource` after completion,
where archive and file identity are available, and `deleteArchiveRes` before
teardown. It edits the source array after J3D load/fixup, not during the
`loaderBasicBmd` return. This is source-supported but not visually proven.

## Read-only resource catalog

`src/game/geometry_probe.cpp` uses a post-hook on `dRes_info_c::loadResource`
to inspect the archive's completed `mRes` table. This level of the call chain
supplies both archive and file names, which `loaderBasicBmd` itself lacks. A
persisted **Log model catalog on resource load** toggle in the Geometry research
panel is OFF by default. When enabled before a model loads, the hook logs BMD
node tag, archive/file, position/normal counts, normal GX type and component
count, PC normal stride, NBT presence, envelope count, shape count, and material
count. It does not retain pointers or mutate game memory. It examines only
future resource loads; already loaded models require an archive/scene reload.
The hook is removed at mod shutdown. A later isolated `D_MN04` run with the
catalog toggle enabled logged the target as BMDR, F32 XYZ, stride 12, eight
J3D-reported normal records, no NBT/envelopes, and one shape/material.

## Supplied game data and named PoC model

The user-supplied RVZ at `C:\dev\Dusklight_Runtime` is GameCube USA GZ2E01 rev 0.
The read-only `tools/inspect-bmd.py` inspected its extracted files in ignored
`build/runtime-assets`; no extracted game data is tracked. Across 2,511 BMDs in
`files/res/Object`, the exact allowlisted target is
`L_mbox_00.arc/l_metabox_00.bmd`: BMDR, 35,040 bytes, F32 XYZ normals with
stride 12 and eight J3D-reported records, normal VTX1 offset 224, no NBT, no weighted
envelopes, and one shape. The game's `d_a_obj_metalbox.cpp` loads this archive
and model for the `ironbox` actor. A byte search found `ironbox` in
`D_MN04/R07_00.arc` and `F_SP116/R03_00.arc` room data. These are candidate
test rooms, not confirmed rendered metal-box sightings.

`geometry_mutation_test` is persisted and OFF by default. When enabled before
the target archive loads, the post-load hook checks the exact archive/file,
F32 XYZ normal format, stride, count, absence of NBT and envelopes, finite
values, and that the full array lies inside the original archive resource.
It backs up those bytes and negates each normal component once. A pre-unload hook
restores them before archive teardown; shutdown and disabling the toggle also
restore them. Disabling after model instances exist may leave copied/transformed
normals in those instances, so a scene reload is required for a clean comparison.
All other models are bypassed. The experiment adds no per-frame work.

The supplied Dusklight v1.4.1 executable was built from June 2026 revision
`f5642f307384bd4b7b7a09f690e391152445c815`; MidnaFX targets the pinned
September 2026 SDK revision above. The older runtime lacks the services used by
the mod, so it cannot validate this PoC. A source-matched Windows host was built
from the pinned revision. In an isolated D3D11 run with `--stage D_MN04,7,0,-1`
and `--cvar mod.com_midnafx_midnafx.geometry_mutation_test=true`, Dusklight
activated MidnaFX, loaded `l_metabox_00.bmd` (35,040 bytes), and MidnaFX logged
`normals forced upward` once. A 30-second run was terminated after this smoke
check, so it did not exercise mod shutdown or restoration. The log demonstrates
the hook reached and changed the named model's source array, but it does not
establish a visible lighting difference, instance propagation, or lifecycle
correctness in gameplay. Gate 1 therefore remains open.

## Gate 1 visual retest and decision

The source-matched Windows D3D11 host was launched visibly into
`D_MN04,7,0,-1` with the same game, stage, render size (1218×928 captured
window), mod package, and default-off grading settings. The first run left
geometry mutation disabled. The next enabled the original upward-normal
diagnostic; a third enabled the stronger negation diagnostic. In both mutated
runs the named BMD loaded and the mutation log appeared once. Graceful Alt+F4
exit from each visible run logged original-normal restoration before the mod
unloaded. This verifies the shutdown restore path in this host; it does not
exercise archive unload/reload, toggle disable, or multiple instances.

The three unmodified game-frame captures are stored locally under ignored
`build/m7-evidence/` as JPEGs. In the visible metal-surface regions, mean
absolute RGB differences against baseline were about 0.7/255 for both upward
and negated normals, near capture/compression noise; Link's animated region
differed by 14 and 26/255 respectively. No unmistakable target lighting change
was seen. The target's first material reported lighting enabled, light mask
255, diffuse mode 2, and GX indexed normals. The J3D count is eight, but offline
inspection sees six ordinary axis normals followed by two very large finite
values at the array boundary. A read-only SHP1/display-list parse found six
GX triangle-strip draws (24 vertex references); normal indices 0–5 each occur
four times, while indices 6–7 are unreferenced. A second candidate room, `F_SP116,3,0,-1`,
opened but did not load this model on its default layer. Parsing its room DZR
showed three `ironbox` ACT2 records at `(3300,-900,5736)`,
`(3450,-900,5736)`, and `(3600,-900,5736)`. `F_SP116,3,13,2` spawns Link
nearby at approximately `(2766,-900,6147)` and does load the model. Paired
baseline/negated captures from that spawn still lacked a clear lighting
change in the nearby box-shaped objects. Rain and Link animation caused
larger uncontrolled pixel differences than the initial dry-scene comparison.
The large visible crates are not those actors. A temporary pre-draw hook logged
all three `daObjMBox_c::Draw` calls at the DZR coordinates, then skipped their
draws for one capture; the crates remained visible. The alternate upward-normal
capture also lacked an unmistakable change. These two JPEGs are retained in
ignored `build/m7-evidence/ironbox-room3-skipped.jpg` and
`ironbox-room3-upward.jpg`. The temporary hook and alternate mutation were
removed; neither is part of the shipped mod.

At this checkpoint, source-array mutation and graceful shutdown restoration
were established, but the actual model was obscured. The subsequent controlled
proof below resolves the renderer question. The shape's referenced normal
indices are 0–5.

## Gate 1 controlled visible-model proof

On the same source-matched Windows D3D11 host, a temporary draw hook projected
the three `daObjMBox_c` centers in `F_SP116,3,13,2` to approximately
`(288,107)`, `(317,89)`, and `(342,73)` in the 640×480 game viewport. This
located them behind the large scenery crates in the earlier captures. For a
repeatable renderer experiment, the hook changed only the first actor's model
draw transform from `(3300,-900,5736)` to `(3000,-900,6000)`, placing its
metal box unobstructed in front of Link. It did not edit game assets, actor
position, collision, or normal data. The temporary hook has been removed from
the shipped mod; the existing resource-load normal mutation remains once per
load and default-off.

Two temporary diagnostic builds used the same stage/spawn, camera, render size,
and model draw transform; the second allowed relocation when the normal
mutation toggle was off. In the original-normal capture, the box's near face and metal edges
are lit; in the negated-normal capture, those surfaces become visibly dark.
The captures are retained locally in ignored
`build/m7-evidence/relocated-original.jpg` and
`build/m7-evidence/relocated-negated.jpg`. A fixed 95×90-pixel region inside
the box's near face averaged 39.3/255 RGB with original normals and 17.9/255
with negated normals (mean absolute difference 21.7/255). A nearby wood-crate
control region changed by 3.2/255. Rain, animation, and JPEG compression make
these measurements approximate, but the model-local lighting difference is
unmistakable. The mutated run logged the exact-resource mutation once and
original-byte restoration on graceful exit. A following original-normal run
rendered the lit face again.

**Gate 1's binary renderer proof passes for this named rigid model:** editing
the allowlisted source normal array before instance creation reaches TP's
normal rendering path. The temporary relocation is a controlled visibility
aid, so it does not establish natural-camera appearance, skinned-model
behavior, archive unload/reload, in-process toggle restoration, or mod reload.
Those remain runtime validation items before broad model coverage. No engine
API change is currently required for this narrow interception point.

## Gate 2 source reconnaissance

`J3DShapeDraw::countVertex` and `addTexMtxIndexInDL` use Aurora's stride-only
`aurora::gx::dl::Reader` on PC (`J3DShapeDraw.cpp:33–105`). Its full-layout
constructor takes the shape's `GXVtxDescList` and vertex format list.
`DrawCmd::attr_idx` reads GX_INDEX8 or GX_INDEX16 indices and `DrawCmd::index`
reads optimized `GX_AURORA_DRAW_INDEXED` indices
(`extern/aurora/include/aurora/dl.hpp:12–84`,
`extern/aurora/lib/gx/dl.cpp:142–160, 224–299`). The existing
`expand_triangles` handles GX_TRIANGLES, GX_TRIANGLESTRIP with alternating
winding, GX_TRIANGLEFAN, and GX_QUADS. Degenerate triangles still need geometric
filtering. Position and normal attributes can carry different indices. Any future
topology pass must traverse every shape and matrix group of one `J3DModelData`,
retain material/shape/group context, and reject missing or unsupported formats.

The PC shape constructor may optimize raw DLs into indexed Aurora commands before
MidnaFX sees them, so a parser restricted to fan/strip opcodes would be wrong.
Aurora's `Reader` symbols are not linkable from the mod SDK (a direct use
produced unresolved externals). MidnaFX therefore has a bounded, read-only
decoder in `src/game/topology.cpp` that mirrors the pinned Reader's command and
attribute-layout rules. It fails closed on unsupported layouts, indices,
commands, and display-list sizes. The source-backed choice is necessary for
the mod binary; the independent original-BMD parser below provides a second
implementation for comparison.

## Gate 2 runtime topology proof

With default-off **Log topology for three M7 models** enabled, the
source-matched Windows host loaded `F_SP116,3,13,2`. The runtime decoder
visited every shape and matrix group of these three exact resources. PC
construction had optimized the original GX triangle strips into Aurora
`DRAW_INDEXED` triangle commands before MidnaFX's resource-load hook ran.
`tools/inspect-topology.py` separately read the original BMD SHP1 lists and
expanded their strips. For each nondegenerate triangle, both decoders hashed
shape, matrix group, and all three ordered position/normal index pairs. The
64-bit hashes match exactly:

| Resource | Original strips | Runtime indexed draws | Triangles | Degenerate | Corner hash |
| --- | ---: | ---: | ---: | ---: | --- |
| `L_mbox_00.arc/l_metabox_00.bmd` | 6 | 1 | 12 | 0 | `1a30360897392267` |
| `R03_00.arc/model.bmd` in `F_SP116` | 9,205 | 31 | 27,126 | 11 | `e084ce9e7c32a4c2` |
| `Bmdl.arc/bl.bmd` (enveloped Link body) | 1,023 | 37 | 3,617 | 0 | `fefc475b8f9a6309` |

The comparison also matched position and normal array counts, unique
referenced indices, and positions with multiple normal indices. The room
model uses S16 XYZ positions/normals, while the metal box uses F32 XYZ;
Link's body has 133 envelope matrices and S16 XYZ normals. Runtime decode
took about 8 µs, 4.6 ms, and 0.6 ms respectively on this Windows host in
one sampled load; these are diagnostic timings, not production benchmarks.
The runtime JSON lines are in ignored `build/runtime-smoke/stdout-gate2-b.log`.

**Gate 2 topology reconstruction PASS for these three classes.** The ordered
per-corner position/normal stream survived PC optimization exactly. This is a
read-only gate result; it does not establish that smoothing is safe or visually
better. The next phase must build model-wide adjacency, preserve intentional
normal/material splits, and reject normal-index conflicts before writing data.
Raw game assets and runtime logs remain ignored and untracked.

## First smoothing prototype and validation boundary

Gate 2 passed before any smoothing write. The subsequent dry run analyzed loaded
models in `F_SP116` and `D_MN04`. The room (`R03_00.arc/model.bmd`) and Link body
(`Bmdl.arc/bl.bmd`) need 37,728 and 3,695 normal-index splits respectively;
the algorithm rejects those models rather than choosing between incompatible
directions for the same normal index. The metal box has no smoothing changes.
Three other conflict-free changes were flat effect meshes (wood-crate debris,
heart effect, portal plane), so they are not suitable evidence of improved
curved-surface shading.

The first write candidate, `@bg0016.arc/model0.bmd` in `D_MN04`, passed
topology and conflict checks and restored on shutdown. Matched settled views
(`build/m7-evidence/smoothing-off-settled.jpg` and
`smoothing-on-settled.jpg`) showed only about 0.8/255 mean absolute difference
over its visible thin panel, comparable to background noise. This is not a
curved-surface improvement. It was removed from the write allowlist.

Three further live scans (`F_SP103`, `F_SP108`, `D_MN05`) found one useful
curved candidate: `OBJ_GM.arc/k_kumo_tubo01.bmd`, a hanging Forest Temple pot.
In `D_MN05,19,0,-1`, the independent original-BMD decoder and runtime decoder
agreed on corner hash `2d260cc6b2cbe6a5`, 174 triangles, 90 positions,
357 normals, one shape, and no degenerate triangles. The original 91 GX
strips become one Aurora indexed draw at the interception point. Normals are
S16 XYZ with 14 fraction bits; positions are F32 XYZ; there are no envelopes.
The default-off write experiment is now confined to this exact archive, file,
topology hash, counts, and encoding. It also requires array bounds, zero index
conflicts, and the pre-unload hook before changing data. The original 2,142
normal bytes are backed up. Diagnostic mutation and smoothing cannot overlap.

In the live source-matched host, this resource produced 129 smoothing groups,
300 changed normal indices, zero conflicts, and zero ambiguous faces. A sample
load recorded 71 µs for topology decode and 77 µs for smoothing planning
(`build/runtime-smoke/stdout-pot-room19-on.log`). A later instrumented live
load measured 65 µs for topology decode, 1 µs for normal-array decode, and
77 µs for smoothing planning, of which 25 µs built adjacency. Total MidnaFX
processing through mutation, before report formatting, was 157 µs
(`build/runtime-smoke/stdout-pot-performance-final.log`). These are single
host samples, not worst-case budgets. The largest tracked vector-storage
snapshot was 35,004 bytes, plus the retained 2,142-byte backup cache (one
entry). The vector figure counts capacities in the decoder and planner; it
excludes allocator metadata, logging, and engine-owned resources, so it is
not a process-memory peak. Processing occurs on resource load, with no
per-frame topology work.

Matched room-19 captures are retained locally as
`build/m7-evidence/pot-off-room19.jpg` and `pot-on-room19.jpg`, with an
enlarged original/smoothed comparison at `pot-compare-enlarged.jpg` (original
left). Visible pot-surface regions differed by about 6–13/255, while adjacent
wall and floor controls were close to zero. The right-hand view softens some
facet shading on the curved body while the mouth rim remains distinct. This
is a narrow visual indication, limited by the roughly 40-pixel pot size,
texture, particles, and possible swing-phase differences; it is not art
sign-off or evidence for global smoothing.

On 2026-09-18, a restored Windows desktop session allowed GPU-window capture
through Windows.Graphics.Capture after direct GDI capture had returned a black
surface. A fresh source-matched D3D11 off/on launch at
`D_MN05,19,0,-1` produced ignored PNG evidence at
`build/runtime-smoke/desktop-pot-off-20260918.png` and
`desktop-pot-on-20260918.png`. Both show Link and the hanging pots in the
same room and camera setup. The enabled log records 300 changed normals,
zero index conflicts, five model instances, and original-normal restoration
on graceful close (`stdout-desktop-capture-on.log`). This confirms that the
live visual capture route is available again, but the small, swinging pot
and different Link pose still limit pixel-level comparison; the prototype
remains default off.

The source actor maps type-1 `Obj_gm` instances to this BMD. Room 19 contains
five such placements. A temporary model-create diagnostic in the final
prototype logged **five successful instances** sharing one preprocessed
`J3DModelData`, followed by one original-byte restoration at shutdown
(`build/runtime-smoke/stdout-pot-instances.log`). A separate temporary
diagnostic used the actual config service to disable the feature after the
fifth instance was created. The settings callback restored the original
normals immediately while the instances were live, and shutdown did not
restore them a second time (`stdout-pot-toggle-test.log`). That forced trigger
was removed after the test. A separate temporary stage-departure trigger
exercised the archive pre-delete hook: the active backup was restored before
the pot resource was deleted (`stdout-pot-transition-test2.log`). The trigger
was removed. Its scripted return request stalled on a black transition view
before the pot reloaded. A later isolated run instead moved from Forest Temple
room 19 to room 0 and back, waiting for room 0 to load before returning
(`build/runtime-smoke/stdout-pot-room-transition-test.log`). The log shows
three smoothing applications: five instances in room 19, two in room 0, and
five after returning to room 19. Each room transition logged archive
pre-delete restoration followed by original-byte restoration before the next
application. Graceful shutdown restored the final backup once. The temporary
transition trigger was removed and the shipped mod rebuilt. This verifies
archive unload/reload in the source-matched host.

A separate host-only test queued Dusklight's normal native-mod reload after
the five room-19 instances had been created
(`build/runtime-smoke/stdout-pot-mod-reload-test.log`). The loader called
MidnaFX shutdown, which restored the original normals once, then reloaded
the package and initialized it successfully. The game ran for several more
thousand frames and exited cleanly. The host-only trigger was removed and
the source-matched host rebuilt. A combined isolated run then reloaded the
mod, moved to room 0, and returned to room 19
(`build/runtime-smoke/stdout-pot-post-reload-test.log`). The already loaded
pot remained original immediately after reload, as expected. The reloaded
mod's resource hook smoothed the newly loaded pot in room 0, restored it on
archive pre-delete, and smoothed the fresh room-19 resource. Shutdown
restored that final backup. Both test-only triggers were removed and the
normal mod and host rebuilt.

No currently scanned enveloped model can safely receive changed normals
without index splitting: Link body, face, and head, horse, enemies, and sampled
NPC models report hundreds or thousands of normal-index conflicts; others
have unsupported normal layouts. Thus the required skinned-model write proof
remains blocked by the existing representation, not by unvalidated guesswork.
Skinned topology read-only proof remains valid. The next geometry milestone
must either find a conflict-free skinned resource or explicitly design safe
normal-index duplication and display-list rewriting before attempting that
proof. The engine ownership and validation boundary for that separate work
is recorded in `geometry-index-splitting-design.md`. The pot toggle stays off
by default.

The dedicated correctness pass checked array bounds against the expanded BMD
resource, exact topology/format identity before writes, finite normal encoding,
index conflicts, material/original-normal splits, and restoration ownership.
It found and fixed the incomplete toggle restore path and added a checked S16
codec with round-trip tests. The follow-up review also checked the new
model-instance hook, exact allowlist replacement, and single-owner backup.
The performance-instrumentation review checked that the extra counters are
load-time only, the vector-capacity arithmetic uses 64-bit storage, and the
reported vector bytes are not presented as a whole-process allocation peak.
Remaining risks are small-scale visual evidence and unresolved skinned
normal-index conflicts. All eight Windows Release tests pass for the current
prototype.
