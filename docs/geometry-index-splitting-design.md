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

## Smallest safe boundary to investigate

The next experiment needs an engine-owned, load-time transaction before model
instances and skin-deformation mappings are initialized. It should accept a
validated per-corner normal assignment, allocate a new normal array with an
explicit count and lifetime, and rebuild every affected shape draw from a
copy. For an Aurora indexed draw, duplicate vertex-table rows when corners
need different normal indices, then rewrite only their index-buffer entries.
Preserve all other attributes, matrix-group boundaries, shape/material
association, and triangle order. If an 8-bit attribute index would overflow,
either promote the descriptor and every vertex stride consistently or reject
the resource. Reject 16-bit overflow, unsupported commands/normal formats,
and any topology hash mismatch before publishing the replacement.

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
2. Build an offline split/reindex transform for that exact resource. Decode
   both old and transformed lists independently; require identical ordered
   triangle positions, shape/material/matrix groups, and all non-normal
   attributes. Require the new normal index at every corner to match the
   intended smoothing group.
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
