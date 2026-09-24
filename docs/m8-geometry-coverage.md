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

This checkpoint does not qualify the other scanned Forest Temple objects, add a
second character, or make the control user-facing beyond renaming the existing
experimental rigid toggle. Natural-placement proof for this rock is impossible
with the inspected official room data, and the controlled rendering proof is
intentionally classified as safety evidence rather than strong art evidence.
The next checkpoint should select a naturally instantiated static/environmental
model with useful curved adjacency and validate it under distinct lighting. The
unused second rock variant is a poor next target. Broader character coverage
still requires a separately fingerprinted identity split and close visual review.
