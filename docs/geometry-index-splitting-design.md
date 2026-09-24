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
verify CPU skinning. The source-matched loader result below advances this
gate; it does not authorize a shipped runtime write.

## Source-matched Windows loader checkpoint (2026-09-18)

With an active desktop session, the pinned Dusklight `edf42c6` host ran on
Windows/D3D11 from the supplied USA game image. A temporary, host-only loader
branch substituted the validated 273,216-byte Link BMD for the original
165,088-byte `Bmdl.arc/bl.bmd`; its storage stayed alive for the process.
The actual `J3DModelLoader`/PC draw construction completed, one Link
`J3DModel::entryModelData` call reported 13,136 normal-array entries and flags
`524288`, and the game continued beyond frame 1,200 before graceful close.
The isolated logs are `build/runtime-smoke/stderr-link-reindex-instance.log`
and `stdout-link-reindex-instance.log` (ignored local evidence). There was no
loader assertion or GPU validation error. The unrelated missing `gczelda2`
card-file warning and device-destroyed messages on shutdown also appeared in
baseline runs. The temporary upstream source edits were removed, its checkout
is clean, and the ordinary source-matched executable was rebuilt.

This is an **identity split**: every new normal copies the original value, so
it cannot show smoother shading. The Link instance did not invoke
`J3DModel::setSkinDeform`; therefore CPU normal-to-matrix mapping and CPU
skinning remain untested. Direct GDI capture returned a black GPU surface;
Windows.Graphics.Capture worked in the subsequent pot off/on retest, but no
Link identity-split visual A/B was recorded. A production
transaction still needs owned resource lifetime, a real smoothing-group
assignment, fail-closed validation, and animation/visual comparison.

## Archive-heap ownership checkpoint (2026-09-18)

A second temporary branch tested the intended replacement lifetime. During
`Bmdl` loading, it allocated the validated 273,216-byte Link replacement from
`JKRHeap::getCurrentHeap()`, copied the rebuilt BMD, and passed that pointer to
`J3DModelLoaderDataBase::load`. The archive's `mDataHeap`, current heap, and
`JKRHeap::findFromRoot(replacement)` all reported the same `JKRSolidHeap`
(`SLID`). The resulting `J3DModelData::getRawData()` equaled the replacement
pointer. The game reached the loaded scene and frame 30 without an assertion or
GPU validation error. The ignored local evidence is
`build/runtime-smoke/stdout-owned-link{,-long}.log`.

This establishes that the existing loader call runs while the archive resource
heap is current and that an early replacement can share the archive's lifetime.
`dRes_info_c` destroys that solid heap only after `deleteArchiveRes`; therefore
the replacement does not need mod-owned storage and cannot become dangling only
because the native mod reloads. The temporary host diagnostics were removed,
the pinned checkout is clean, and the ordinary host was rebuilt.

A follow-up on 2026-09-23 moved the same transaction into a temporary MidnaFX
pre-hook and used the room-scoped Forest Temple pot. The hook matched the exact
12,896-byte source BMD, copied the validated 17,600-byte identity-split rebuild
into the current archive heap, and replaced the loader argument. Runtime logs
showed that `JKRHeap::findFromRoot(replacement)` equaled the current heap. At
frame 300, an automated room-19 to room-0 transition destroyed that exact heap.
The resource then loaded again into a different archive heap and received a new
replacement. The game continued without an assertion, GPU validation error, or
stale-pointer failure. Evidence is in ignored local log
`build/runtime-smoke/owned-pot-logs-4/dusklight-20260923-112406.log`.

Archive-heap replacement ownership therefore passes for load, unload, and
reload. A new host allocation API is not needed for this boundary. Production
code still needs an in-process validated transformer; the temporary hook read
an offline-generated identity-split file only to isolate lifetime behavior. All
temporary mod code was removed after the run.

## In-process transformer checkpoint (2026-09-23)

`src/game/bmd_rebuild.cpp` now ports the validated identity-split rebuild to
bounded C++. It parses VTX1/SHP1, expands triangles, strips, fans, and quads into
three-corner strips, skips zero-area triangles, appends unique 16-bit normal
indices, rebuilds affected offsets and draw tables, and independently reparses
the result before returning bytes. Unsupported encodings, direct or 8-bit normal
indices, malformed commands, bad bounds, duplicate draw entries, overflow, and
topology mismatches return an error without publishing output.

The C++ output is byte-for-byte identical to the Python proof for both sampled
resources. The pot SHA-256 is
`530543fbe46a43e23247853cce5d588fcb0419afacd9d2f8151e16894512525e`;
Link is
`4d8681d59e6240f033a4e184294be0ae31a6ecea3b136bbd6fc2246e692b60ee`.
Portable tests cover the successful rebuild, alternating strip winding,
input immutability, degenerate removal, unsupported index width, invalid normal
indices, and malformed GX commands.

The default-off pot smoothing path now invokes this transformer from the
`dRes_info_c::loadResource` pre-hook. It accepts only the exact
`OBJ_GM.arc/k_kumo_tubo01.bmd` source (12,896 bytes, FNV-1a
`a9efd2ace652b900`), allocates the 17,600-byte validated output in the current
archive solid heap, and substitutes that pointer in a
`J3DModelLoaderDataBase::load` pre-hook. All other resources and all failed
checks retain their original pointer. The smoothing post-hook then requires
the rebuilt topology fingerprint before writing normals.

A source-matched live run decoded 174 triangles, wrote 879 normals into a
capacity of 880, and completed the rebuild and archive copy in 184 microseconds.
The rebuilt runtime topology contained 90 positions, 880 normals, 522 referenced
normal indices, 129 smoothing groups, 450 changed normals, and zero index
conflicts. Five instances shared the processed model data. Graceful shutdown
restored original normal values.

An isolated room-19 to room-0 transition then restored the active normal backup,
released replacement tracking before archive destruction, and rebuilt the fresh
resource in a different archive heap. The two measured rebuilds took 191 and
235 microseconds. No assertion, fatal error, GPU validation error, stale pointer,
or double processing was observed. The transition hook was removed and the
ordinary mod package restored. Local ignored evidence is under
`build/runtime-smoke/inprocess-rebuild-1` and
`build/runtime-smoke/inprocess-rebuild-lifecycle`.

## Skinned Link-body checkpoint (2026-09-23)

A separate hidden `geometry_skinned_smoothing` switch, also default off,
allowlists the exact currently loaded Link body `Kmdl.arc/al.bmd` (140,448
bytes, FNV-1a `c00c6d9abd5d79bc`). The in-process transformer produced a
222,688-byte archive-owned resource with 10,064 per-corner normal entries and
2,777 triangles. The source-matched loader and Aurora optimizer accepted it.

Runtime reconstruction reported 1,560 position-array entries, 18 shapes, 110
envelopes, 39 optimized indexed draws, 2,777 triangles, 8,331 referenced normal
indices, corner hash `6ed5b0df43c35b63`, 3,358 smoothing groups, 6,855 changed
normals, and zero normal-index conflicts. The measured rebuild was 2.306 ms;
runtime decode plus smoothing and mutation was 2.336 ms; tracked working vectors
peaked at 788,168 bytes and the restoration backup used 60,384 bytes. One live
model instance used the processed model. The game ran through 3,600 logged
frames and shut down cleanly, restoring original normal values. No assertion,
fatal error, or GPU validation error appeared. Evidence is the ignored local
`build/runtime-smoke/stdout-link-smoothing-live.log`.

This passes resource ownership, topology, index-conflict, sustained execution,
and graceful restoration for one enveloped model. A follow-up host-only probe
also established the active skinning path. The model used flags `0x00080000`,
both CPU skin flags were clear, `getSkinDeform()` was null, and the rebuilt
source normal array remained the current vertex-buffer normal pointer. Across
draws 1, 30, and 120, its 134 GPU normal matrices changed from hash
`2e2b294129dc7c70` to `c5956fd5277c0f29` and `bb4995530c60a3aa`, while the
source pointer remained current. Thus the existing GPU matrix path consumes the
smoothed source normals through animation. The temporary host probe was removed;
local evidence is `build/runtime-smoke/stdout-link-transform-host-2.log`.

Visual normal quality is still unproven: the desktop automation provider did
not expose the Dusklight window, so no matched original/smoothed capture was
made. Matrix movement proves animated consumption, not improved shading or
preserved hard edges. Keep the switch hidden and off by default until visual
comparison passes. The rigid pot remains the only user-facing smoothing
experiment.

The separate CPU check in `docs/notes/cpu-skinning-check.md` found two pinned
host blockers: Aurora's optimized PC draw commands cause the CPU normal mapper
to visit zero corners while returning success, and its GameCube branch reads
raw big-endian strip counts incorrectly on PC. With both paths corrected only
in an isolated test host, original and per-corner-reindexed `B_bh.arc/bh.bmd`
completed `setSkinDeform` and one `J3DModel::calc` with non-null normal buffers;
the split sample mapped all 5,817 triangle corners without matrix conflicts.
This is CPU representation feasibility evidence, not a pass for the unmodified
host or an animated character.

## Smallest safe boundary to investigate

The current source path is `dRes_info_c::loadResource` obtaining raw bytes
from `JKRArchive::getIdxResource`, then calling either
`dRes_info_c::loaderBasicBmd` or `J3DModelLoaderDataBase::load` before
`J3DModelLoader` reads VTX1/SHP1. The mod hook dispatcher passes arguments by
reference to pre-hooks, so a pre-hook on `J3DModelLoaderDataBase::load` can
substitute a validated input pointer early enough for the existing loader and
Aurora optimizer. The archive-heap checkpoints confirm the replacement can use
the current archive solid heap rather than mod-owned storage and survives the
required load/unload/reload lifecycle. Failure must leave the original pointer
untouched.

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
2. The rebuilt BMD passed the source-matched loader, PC draw construction,
   and one Link instance creation. An isolated, corrected-host CPU test passed
   mapping and one calculation on `bh.bmd`. Resolve the two pinned host CPU
   blockers and repeat in the unmodified production path, then replace the
   identity split with indices assigned by the intended smoothing groups.
   Require identical ordered triangle positions, shape/material/matrix
   groups, and all non-normal attributes after the transform.
3. Port the validated rebuild into a fail-closed in-process transformer. Allocate
   its committed output from the current archive heap. Test allocation failure,
   index-width limits, malformed lists, multiple instances, archive
   unload/reload, and mod reload. The room-scoped runtime proof established that
   no new host allocation API is needed.
4. In the source-matched game, compare a close original/smoothed view through
   several animations. Check silhouette and hard edges, skinning motion,
   inverted/exploding lighting, Twilight and normal-world lighting, and
   restoration/lifetime behavior. Keep character smoothing default off until
   that visual and lifecycle proof passes.

This is a separate topology-mutation milestone. M7's no-index-rewrite rule
continues to apply to the current shipped prototype.
