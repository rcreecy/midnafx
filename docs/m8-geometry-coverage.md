# M8 geometry coverage

## First checkpoint: one additional static model

M8 broadens the validated geometry set without enabling global smoothing. The
feature remains default off and exact-resource allowlisted. This checkpoint adds
the Forest Temple web-covered rock `OBJ_GM.arc/k_kumo_iwa00.bmd` and changes the
mutation owner from one global backup to a bounded collection of per-model
backups. It does not add a new effect, rewrite positions, or enable smoothing by
default.

The source-matched D3D11 host observed the original rock as 13,856 bytes with
FNV-1a `aae14e74c4cca0b8`, topology hash `4905bff69be3ae64`, 40 positions, 197
normals, one shape, no envelopes, and 67 triangles. Its normal encoding is S16
XYZ with six-byte stride and 15 fractional bits. The exact size, resource hash,
topology hash, counts, envelope state, and normal format must all match before a
write. This differs from the rebuilt pot and Link resources, which use 14
fractional bits; encoding now preserves the validated model's own fraction.

With the existing 55-degree grouping and conservative 25% geometric blend, the
rock produced 49 groups, 55 changed normals, zero conflicts, and zero ambiguous
faces. Decode, normal planning, and mutation took 127 us in the final sample;
tracked vector capacity was 17,164 bytes and the restoration backup was 1,182
bytes. The rebuilt pot simultaneously produced 129 groups and 450 changed
normals. The cache held both entries, and graceful shutdown restored both
backups. Five pot instances continued to share one processed model resource.

The layer-0 room scan loaded the rock resource but created no rock model
instance, so resource processing alone was not accepted as visual proof. A
temporary source-matched host-only probe changed the five room-19 hanging-pot
actor types to the existing rock type. The resulting real actor/render path
created five exact rock instances. `build/m8-evidence/rock-substitution-off.jpg`
and `rock-substitution-on.jpg` are the matched captures;
`rock-substitution-comparison.jpg` crops the hanging models. The web texture
makes the shading change subtle, but the smoothed capture has no new faceting,
silhouette change, or web-pattern corruption. The host-only actor substitution
was removed and the clean host rebuilt after capture. This proves safe rendering
of the new allowlist entry, but it is not strong art-quality evidence of a visible
improvement under varied lighting.

A read-only scan then decoded all 305 extracted room archives and inspected
their ACT/SCO actor records. It found 25 `Obj_gm` placements: Forest Temple uses
types 0, 1, and 5, while the remaining placements use type 15, which the actor
maps back to type 0. No room contains type 3, the only actor type that selects
`k_kumo_iwa00.bmd`. A natural placement therefore does not exist in the official
USA disc data available for this test. Future visual grading of this exact model
must remain an explicitly controlled substitution test.

## Second checkpoint: natural Ordon beehive

The next static target is the naturally placed Ordon beehive
`E_nest.arc/o_hachinosu_01.bmd`. The official `F_SP103` room data instantiates
this actor; the source-matched host created one model instance through its normal
actor and render path. The original resource is 12,576 bytes with source FNV-1a
`de8546d0a1fd387d`, runtime topology hash `ae27261b1928e1f5`, 61 positions, 64
normals, one shape, no envelopes, and 111 triangles. Its normals are S16 XYZ,
six-byte stride, with 15 fractional bits.

The original display list aliases normal indices across incompatible smoothing
groups and reports 28 conflicts. MidnaFX therefore fails closed on the original
resource. The identity-split rebuild preserved all non-normal corner attributes
and expanded 397 referenced normal entries into a capacity of 400. Its exact
runtime topology hash is `d107e10bca9ec9d0`. The runtime decoder found 111
triangles, zero degenerates, 59 referenced positions, 333 referenced normal
indices, 75 smoothing groups, zero conflicts, and zero ambiguous faces.

At 55 degrees with the conservative 25% blend, smoothing changed 62 normals.
The final instrumented sample used 114 us for decode and 203 us total for decode,
adjacency, planning, and mutation. Peak tracked vector capacity was 31,064 bytes;
the restoration backup was 2,400 bytes. Exact resource and topology fingerprints,
counts, envelope state, and normal encoding all gate mutation.

The natural nest hangs outside the spawn camera view. A temporary host-only probe
forced its saved broken-state switch off, enlarged it, and kept it in front of
Link for matched default-off/default-on captures. The same natural actor and real
render path remained in use. The enlarged comparison showed a small, coherent
reduction in faceted lighting across the curved shell with no new triangle
outlines, silhouette change, texture corruption, or hard-edge loss. Local
evidence is in `build/m8-evidence/nest-off.jpg`, `nest-on.jpg`, and
`nest-comparison.jpg`. The visibility probe was removed afterward.

Three launches rebuilt and processed the resource once each. Graceful shutdown
restored its 2,400-byte normal backup each time. The final run used the clean
source-matched host after removal of all visibility instrumentation. This proves
the production actor path and restoration behavior for the new target. Existing
multi-entry room-transition evidence continues to cover concurrent backup
ownership; concurrent rebuilt-resource ownership remains a source-reviewed path.

## Third checkpoint: natural Ordon pumpkins

The next target is the naturally visible Ordon pumpkin
`pumpkin.arc/pumpkin.bmd`. Spawn `F_SP103,0,27,0` places several instances in the
camera view without actor relocation, scale changes, or host instrumentation.
The official resource is 17,824 bytes with source FNV-1a
`9b2ddb5ecbd95421`. Its original runtime form has 306 positions, 330 normals,
two shapes, no envelopes, 495 triangles, and topology hash
`2e5b98d2bbc10f66`. Normals use S16 XYZ, six-byte stride, and 15 fractional
bits. The original indices report 514 smoothing conflicts, so direct mutation
remains prohibited.

The identity-split rebuild preserves all non-normal corner attributes and writes
1,815 referenced normals into a capacity of 1,818. Its exact runtime topology
hash is `2b808b13bc7ab6e8`; the rebuilt resource hash observed after loading is
`2058ca886be0a019`. Runtime analysis found 495 triangles, zero degenerates, 305
referenced positions, 1,485 referenced normals, 604 smoothing groups, zero
conflicts, and zero ambiguous faces. Exact source and rebuilt fingerprints,
counts, envelope state, and normal encoding gate every write.

This resource is a packed child archive. Dusklight loads it synchronously with
`dRes_info_c::setRes(JKRArchive*, JKRHeap*)`, which intentionally leaves
`mDataHeap` null while the parent archive's solid heap remains current. MidnaFX
now tracks the nested resource-load owner and allocates the rebuilt bytes in that
same current heap. The rebuilt bytes therefore share the lifetime and allocation
domain of the J3D data that points into them; no separate free or engine change is
required.

At 55 degrees with the 25% conservative blend, smoothing changed 1,066 normal
entries. Rebuild took 427 us. Runtime decode took 172 us, and decode, adjacency,
planning, and mutation took 466 us total. Peak tracked vector capacity was
145,200 bytes, the replacement resource was 32,448 bytes, and the restoration
backup was 10,908 bytes. Twelve natural actors shared the one processed model
resource.

Matched default-off and default-on captures are in
`build/m8-evidence/pumpkin-off.png`, `pumpkin-on.png`, and
`pumpkin-comparison.png`. The camera, spawn, backend, resolution, resource, and
lighting state match; Link and the chicken continued their idle animations. The
curved pumpkin lobes show a coherent shading change with no new triangle
outlines, silhouette change, texture corruption, or hard-edge loss. Default-off
runtime loaded the same twelve actors without rebuilding or smoothing. Graceful
shutdown of the enabled run restored the original normal backup.

## Lifecycle and safety review

Each mutation entry owns its model pointer, resource owner, original byte copy,
byte count, instance count, and smoothing class. Resource deletion restores only
entries owned by that archive. Shutdown restores every entry. Rigid and skinned
settings restore their own classes so disabling one cannot undo the other.
Malformed delete callbacks with no owner do nothing. The diagnostic normal test
remains mutually exclusive with smoothing.

The review also checked exact resource identity, normal-array bounds, native
fraction encoding, finite output, conflict failure, vector erasure during
multi-entry restoration, and model-instance lookup after vector reallocation.
No per-frame work was added. Portable tests passed 7/7 and Windows Release tests
passed 9/9.

The two-entry lifecycle was exercised in the source-matched D3D11 host with a
temporary host-only room-19 to room-0 transition after 300 gameplay frames. In
room 19, the rock and pot produced cache entries one and two, and five pot
instances shared the processed resource. Before archive deletion, one pre-delete
event restored both backups and released the rebuilt pot resource. Room 0 then
loaded a fresh archive: the rock and pot were each processed once again, cache
entries returned to one and two, and two pot instances were created. Graceful
shutdown restored both fresh backups. The run therefore recorded two
applications per model, four restorations total, and no error, fatal, assertion,
GPU-validation, stale-pointer, or double-processing report. The transition
trigger was removed and the clean source-matched host rebuilt afterward.

## Boundary

M8 now covers two naturally instantiated curved static actors as well as the
controlled Forest Temple rock. Pumpkin proof uses an unmodified camera-visible
placement and is stronger visual evidence than the relocated beehive test. M8
does not qualify other scanned objects, add a second character, or enable
smoothing by default. The next checkpoint should grade these allowlisted models
under additional natural lighting and room transitions before expanding static
coverage. Broader character coverage still requires a separately fingerprinted
identity split and close visual review.
