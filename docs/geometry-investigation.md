# M7 geometry investigation — Gate 1 checkpoint

Pinned Dusklight revision: `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
CameraService and camera overrides remain deferred. This checkpoint contains source
research only. It does not enable geometry processing or claim a rendered result.

## Decision

**Stop before the controlled mutation PoC and adaptive smoothing.** Source inspection
identifies a plausible normal stream, but the required binary test — an unmistakable
lighting change on one explicitly identified model through TP's actual renderer —
cannot be performed without an installed game resource and a Dusklight runtime. No
`.arc`, `.bmd`, or `.bdl` game asset is present in this workspace, so even the test
model cannot be identified and checked against its actual format. Gate 1 is therefore
incomplete, not passed. Gate 2 and smoothing implementation must remain pending under
the M7 stop condition.

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

The buffer is writable in the loader's PC fixup path, but **safe restoration on
mod reload, shared instances, and skinning is not yet established**. A bare
`loaderBasicBmd` return hook cannot identify a file by name: its arguments are only
the node tag and raw pointer. An allowlisted experiment needs a higher-level
archive/file association, verified against an actual asset, before mutation.

## Interception candidates

| Point | Ordering | Limitation |
| --- | --- | --- |
| End of `J3DModelLoader::readVertex` | After PC endian fixup, before SHP1 | Engine-wide virtual/member hook, no complete shape or resource name yet. |
| End of `J3DModelLoader::load` | After shape setup, before caller's shared material DL | Engine-wide, no archive/file identity; may include unsupported loader paths. |
| `dRes_info_c::loaderBasicBmd` return | Narrow game-side path, after complete model load | After shared material DL; no file name; misses other BMD node types. |

The third remains the best **candidate** for a restricted PoC, provided a parent
resource context reliably identifies one file, the target is a supported static
model, and teardown restores the original bytes before the archive is released.
It is not selected as a proven mutation hook yet.

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
`Reader` offers the necessary path, but no TP model DL was available here to verify
actual primitive mix, index widths, NBT usage, or format completeness. Gate 2 is
also unproven.

## Required next experiment

1. On a Dusklight-capable installation, identify **one exact archive and BMD file**
   with a static ordinary model. Record its node tag, position/normal GX format,
   normal count/stride, NBT status, envelope count, and shape/primitive summary.
2. Prove the chosen hook runs once for that resource and before any model instance
   copies its normals. Verify how the hook obtains the archive and file identity.
3. Preserve original normal bytes under an archive-scoped lifetime. In a developer
   mode that defaults off, negate the selected model's F32 XYZ normals once at
   load. Restore them before mod detach and archive release. Reject all other
   resources and any uncertain format/lifetime.
4. Capture matched original/negated screenshots and verify an unmistakable
   lighting change. Exercise two instances, archive unload/reload, and mod reload.
5. Only after that proof, validate `Reader` against the chosen model's DLs and
   implement the conservative smoothing subset. Do not infer a successful visual
   proof from compilation or source tracing.

The required engine/API change, if a safe archive/file identity and pre-unload
callback cannot be established with hooks, is a model-resource lifecycle service:
it should supply the archive/file name, post-fixup `J3DModelData`, and a callback
before instances are created and before the resource buffer is released.
