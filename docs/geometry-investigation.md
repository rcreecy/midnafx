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

The default-off write experiment is confined to
`@bg0016.arc/model0.bmd` in `D_MN04,7,0,-1`. An independent raw-BMD parse and
the source-matched runtime decoder agreed on corner hash `d105924d00769e26`:
492 triangles, 312 positions, 1,290 normals, two shapes, no degenerate
triangles. The runtime draws are two optimized indexed commands rather than
the raw BMD's 398 GX strips. Position data is F32 XYZ; normals are S16 XYZ
with 15 fraction bits; there are no envelopes. The prototype checks resource
identity, this exact hash and layout, array bounds, index conflicts, and the
pre-unload hook before writing. It keeps the original 7,740 normal bytes and
restores them on feature disable, archive deletion, or shutdown. Only one
resource backup can be active at a time. Both diagnostic mutation and
smoothing are off by default and cannot overlap.

In a live source-matched host run (`build/runtime-smoke/stdout-smooth-prototype.log`),
the target produced 642 smoothing groups, 912 changed normal indices, zero
index conflicts, and zero ambiguous faces. Decode took 128 µs and smoothing
planning 233 µs in that sample; the write occurred once on load and original
bytes were restored on graceful exit. These times exclude allocation, normal
decode, encoding, and logging, so they are not a complete load-time benchmark.
The backup occupies 7,740 bytes and the active backup cache has one entry;
topology and adjacency allocation peaks have not been measured. There is no
per-frame geometry work. All eight Windows Release tests pass. A second live
run after the checked S16 codec change again logged one application and one
restoration (`build/runtime-smoke/stdout-smooth-final.log`).

**The smoothing prototype is experimental, not visually validated.** The
candidate is a thin dungeon architectural model. We have not captured a
matched original/smoothed pair that demonstrates better curved-surface shading
while preserving its hard edges. Consequently this prototype must remain off
by default and cannot be described as completed M7 smoothing. Skinned-model
mutation, multiple-instance sharing, archive unload/reload, in-process disable,
and mod reload are likewise unproven. The next experiment is to obtain matched
views of the exact allowlisted model with the toggle off/on, inspect the result,
then exercise those lifecycle cases before broadening coverage. If it fails
visually, choose a different genuinely curved, conflict-free model or add an
explicit normal-index-splitting capability in a later milestone.

The dedicated correctness pass checked array bounds against the expanded BMD
resource, exact topology/format identity before writes, finite normal encoding,
index conflicts, material/original-normal splits, and restoration ownership.
It found and fixed the incomplete toggle restore path and added a checked S16
codec with round-trip tests. The remaining risk is validation, not a known
buffer or lifetime failure: this target has no demonstrated visual benefit,
and the lifecycle/skinning cases above have not yet been exercised.
