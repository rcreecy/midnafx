# M7 geometry investigation — controlled mutation checkpoint

Pinned Dusklight revision: `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
CameraService and camera overrides remain deferred. This checkpoint contains source
research, a read-only resource catalog, and a default-off, single-model normal
mutation experiment. It does not claim a rendered result or enable smoothing.

## Decision

**Gate 1 remains open; stop before Gate 2 and smoothing.** The supplied USA GZ2E01
RVZ was inspected locally without adding game assets to the repository. It supplies
an exact static test model and permitted a tightly scoped mutation PoC. The required
binary test is an unmistakable lighting change in TP's actual renderer, with matched
screenshots and unload/reload checks. That visual result has not been obtained.

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
values at the array boundary; whether those last two records are referenced
needs further investigation. A second candidate room, `F_SP116,3,0,-1`,
opened but did not load this model on its default layer. Parsing its room DZR
showed three `ironbox` ACT2 records at `(3300,-900,5736)`,
`(3450,-900,5736)`, and `(3600,-900,5736)`. `F_SP116,3,13,2` spawns Link
nearby at approximately `(2766,-900,6147)` and does load the model. Paired
baseline/negated captures from that spawn still lacked a clear lighting
change in the nearby box-shaped objects. Rain and Link animation caused
larger uncontrolled pixel differences than the initial dry-scene comparison.
The visible boxes have not been tied unambiguously to the three actor records.

The evidence proves source-array mutation and graceful shutdown restoration,
but not that the visible object in the capture is the allowlisted model or that
the renderer consumes the edited normals. **Gate 1 is not passed.** Per the M7
stop condition, Gate 2 topology work and adaptive smoothing remain deferred.
The next useful test is a fixed, clearly identified view of an `ironbox`
instance, with before/after frames and its shape's referenced normal indices
recorded. If that view remains unchanged, the engine needs a model-resource
mutation callback before normal values are captured or uploaded, with archive
identity and a pre-release restoration callback. Current evidence does not
prove such an API change is necessary.

## Gate 2 source reconnaissance, not validation

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
`Reader` offers the necessary path, but its output on the selected model has not
been validated. Gate 2 remains unproven and is gated on Gate 1's visual result.

## Required next experiment

1. In a source-matched Dusklight host, enable **Mutation test: metal box only**
   before entering a candidate room, then reload the room. Confirm the single
   "normals negated" log for the exact resource.
2. Capture matched original/mutated metal-box screenshots and verify an
   unmistakable lighting change. Exercise two instances, archive unload/reload,
   toggle disable, and mod reload, checking restoration logs and visuals.
3. Only after that proof, validate `Reader` against the chosen model's DLs and
   implement the conservative smoothing subset. Do not infer a successful visual
   proof from compilation or source tracing.

The required engine/API change, if a safe archive/file identity and pre-unload
callback cannot be established with hooks, is a model-resource lifecycle service:
it should supply the archive/file name, post-fixup `J3DModelData`, and a callback
before instances are created and before the resource buffer is released.
