# Geometry after M7: normal-index splitting boundary

M7's default-off Forest Temple pot experiment uses existing normal indices.
It does not rewrite vertex topology. Its read-only topology decoder also
validated a skinned Link body, but the planner found 3,695 normal-index
conflicts there; sampled face/head, horse, enemy, and NPC resources likewise
cannot safely receive the proposed normals in their current index layout.
Do not enable smoothing for these resources or treat the pot result as a
character-smoothing proof.

## Why a normal-array write cannot resolve this

A GX corner has an independent position index and normal index. One existing
normal index can be referenced by corners that need different results after
respecting face angle, original hard splits, and material boundaries. Changing
the one array entry changes every reference. The current planner rejects the
whole model rather than selecting one group's result.

At the resource hook, PC `J3DShapeDraw` already holds a copied, sometimes
Aurora-optimized display list (`J3DShapeDraw.cpp`). An optimized indexed draw
has an index buffer and a vertex table: changing only an index-buffer entry
may affect another corner that shares that vertex-table row. The source
normal array lives in the expanded model resource, while the draw list has a
separate JKR allocation. `J3DVertexData` exposes normal count and array
getters but no public replacement operation. CPU skinning also sizes normal
mapping and transformed buffers from the model's normal count and reads
normal indices from shape draws (`J3DSkinDeform.cpp`, `J3DVertex.cpp`). A
mod-only append to the original normal bytes would leave counts, owners, and
skinning buffers inconsistent.

The independent read-only audit (`tools/inspect-topology.py --normal-usage`)
of the supplied USA BMDs found 16-bit normal indices in both Link body and
the Forest Temple pot. Link body has 2,282 normal entries and 3,617 triangles;
72 normal indices are referenced from multiple positions, 10 from multiple
shapes, and 396 from multiple shape/matrix-group pairs. The pot has 357
normal entries and 174 triangles, with none reused across positions, shapes,
or matrix groups. These are raw index-sharing counts, not the number of
required smoothing splits; a shared index can still legitimately use one
direction. They are independent evidence that a Link rewrite must preserve
matrix-group semantics as well as shading groups.

## Offline stream-rewrite feasibility

`tools/prove-normal-reindex.py` now performs a pessimistic, read-only rewrite
in memory. It expands each surviving triangle into a three-vertex GX strip,
gives every corner a unique 16-bit normal index, and copies that corner's
original normal bytes to a conceptual expanded array. The validator parses
the rewritten streams again and requires identical ordered non-normal
vertex records, shape/matrix-group identity, normal values, and triangle
count. It also checks the original topology against the independent BMD
decoder. It never writes a game asset.

On the supplied `Bmdl.arc/bl.bmd`, 1,023 raw strips expanded to 3,617
three-vertex strips across the same 8 shapes and 37 matrix groups. Normal
entries rose from 2,282 to 13,133; the largest index, 13,132, fits in
GX_INDEX16. The aggregate raw draw-list bytes rose from 42,272 (including
source padding) to 84,828 before output alignment. Mapping the rewritten
indices back to their source entries reproduced the independent Link corner
hash `fefc475b8f9a6309`. The pot control also passed: 174 triangles, 357
to 879 normals, and original hash
`2d260cc6b2cbe6a5`. This is a worst-case representation proof, not an
efficient rewrite or a smoothing result. It does not yet rebuild a complete
BMD resource, exercise Aurora's PC optimizer, or verify skinning at runtime.

The three-vertex-strip choice matters: `J3DSkinDeform::initMtxIndexArray`
walks raw GX strips/fans when it builds normal-to-matrix mappings. A generic
GX_TRIANGLES conversion could fall outside that path. The proposed engine
transaction should be placed before `J3DModelLoader::readVertex` and
`readShape`, so their existing array-count fixup and PC display-list
optimization see the same rewritten resource.

`tools/prove-bmd-rebuild.py` now builds that complete BMD **in memory**. It
replaces VTX1's normal array and SHP1's raw display-list section, updates
their block sizes and affected section offsets, rebuilds the outer file size,
and leaves all other blocks byte-for-byte identical. The independent decoder
accepts both rebuilt samples and matches the expected rewritten corner hash:
Link grows from 165,088 to 273,216 bytes with 3,617 triangles; the pot grows
from 12,896 to 17,600 bytes with 174 triangles. The padded normal-array
capacity is 13,136 for Link (13,133 actual entries) and 880 for the pot (879
actual entries). The proof rejects offsets, draw-table bounds, and topology
mismatches. These results show a coherent file representation only. They do
not exercise Aurora's PC optimizer, establish engine buffer ownership, or
verify CPU skinning. A source-matched loader test is the next gate; no runtime
write is authorized by the offline result.

## Smallest safe boundary to investigate

The preferred next experiment is an engine-owned transaction on a validated
copy of the BMD before `J3DModelLoader` reads VTX1 and SHP1. It should accept
a per-corner normal assignment, expand the normal array and raw GX streams,
then let the existing loader build its optimized draw copies. If that early
boundary proves unavailable, a later API would also have to duplicate
Aurora indexed vertex-table rows when corners need different normal indices;
changing only index-buffer entries is insufficient. Preserve all other
attributes, matrix-group boundaries, shape/material association, and
triangle order. If an 8-bit attribute index would overflow, either promote
the descriptor and every vertex stride consistently or reject the resource.
Reject 16-bit overflow, unsupported commands/normal formats, and any
topology hash mismatch before publishing the replacement.

The transaction must update the data seen by both GPU draws and CPU skinning,
including normal count, array pointers, per-instance transformed-buffer
sizes, and normal-to-matrix mappings. It must either commit all replacements
before any instance exists or leave the original resource untouched. The
engine should own replacement buffers through archive teardown. A mod reload
must not free storage still used by live models; a disabled toggle should
defer a new preprocessing choice until the next resource load.

## Proof sequence before implementation in MidnaFX

1. Trace one sampled enveloped resource from raw BMD through PC optimization,
   model instance creation, and its actual GPU/CPU skin path. Record which
   code consumes normal indices and when buffers are allocated. Confirm that
   the proposed transaction point precedes all consumers.
2. Run the rebuilt in-memory BMD through the source-matched loader and PC
   optimizer, then through a sampled model instance and CPU skinning setup.
   Require identical ordered triangle positions, shape/material/matrix
   groups, and all non-normal attributes. Then replace the identity split
   with indices assigned by the intended smoothing groups.
3. Add an engine-owned, fail-closed replacement API only after the offline
   transform proves feasible. Test allocation failure, index-width limits,
   malformed lists, multiple instances, archive unload/reload, and mod reload.
4. In the source-matched game, compare a close original/smoothed view through
   several animations. Check silhouette and hard edges, skinning motion,
   inverted/exploding lighting, Twilight and normal-world lighting, and
   restoration/lifetime behavior. Keep character smoothing default off until
   that visual and lifecycle proof passes.

This is a separate topology-mutation milestone. M7's no-index-rewrite rule
continues to apply to the current shipped prototype.
