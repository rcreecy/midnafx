# M8 first-checkpoint correctness review

The first implementation correctly rejected `k_kumo_iwa00.bmd` because it used
15 fractional bits while the existing rebuilt targets used 14. The encoder and
exact-format gate now retain each model's validated fraction. A second review
found that a shared smoothing kind would let either configuration switch restore
both rigid and skinned entries; backups now carry separate rigid and skinned
kinds, and each callback restores only its own class. A null archive-delete
callback also returns without touching unrelated entries.

The final live rigid run processed the rock and pot resources together, recorded
cache sizes one then two, preserved five shared pot instances, and restored both
normal arrays on graceful shutdown. A room scan then showed that room 19 did not
naturally instantiate the loaded rock. A temporary host-only actor substitution
created five rock instances through the real actor/render path; the smoothed run
completed without new faceting, corruption, stale pointers, or shutdown failure.
The substitution was removed and the source-matched host rebuilt cleanly. No
error, fatal, assertion, or GPU validation line appeared. The existing Link
algorithm, exact allowlist, topology checks, and default-off state are unchanged.

A follow-up decoded the ACT/SCO records in all 305 extracted room archives. None
of the 25 `Obj_gm` placements selects rock type 3, so natural-placement proof is
unavailable rather than merely undiscovered in the Forest Temple layer scan.
The multi-entry lifecycle then passed a controlled room-19 to room-0 transition:
both entries restored before archive deletion, both fresh resources were
processed once after reload, and both restored once more at shutdown. The
temporary transition trigger was removed and the clean host rebuilt.

## Natural beehive checkpoint

Review found the original beehive's 28 normal-index conflicts unsafe for direct
mutation. The identity-split rebuild preserves every non-normal corner field,
then requires exact source and rebuilt fingerprints before any write. Runtime
validation reported 111 triangles, zero degenerates, zero conflicts, zero
ambiguous faces, finite smoothing output, and 62 changed normals. Unsupported or
mismatched resource data continues to fail closed.

Replacing the single optional rebuilt-resource owner with a vector was required
because Link and the beehive can be live together. Lookup uses source or rebuilt
data pointer during model loading and archive owner during topology analysis and
deletion. No pointer into the vector survives a push, erase, or callback. Archive
deletion erases only matching owner entries; shutdown restores all normal backups
before releasing replacement bookkeeping. Duplicate owner/kind creation is
rejected.

Matched live captures used a temporary visibility probe because the natural
actor sits above the spawn camera and the current save marks it broken. The probe
changed only actor placement, scale, and saved-switch handling. It was removed
and the source-matched host rebuilt cleanly. The smoothed capture introduced no
triangle outlines or corruption. Graceful shutdown restored the normal backup.
Final review found no buffer overrun, invalid index, NaN/Inf output, double
mutation, ownership leak, material-boundary leak, hard-edge loss, or unsupported
format write in this checkpoint.

## Natural pumpkin checkpoint

Initial review found that `pumpkin.arc` is a packed child archive and therefore
has no `mDataHeap`. Treating that null pointer as an allocation failure would
silently skip the rebuild. Dusklight source shows that packed children load
synchronously while the parent archive's solid heap is current. The loader hook
now associates nested J3D loads with their exact `dRes_info_c` owner and allocates
the rebuilt bytes in that current heap. This is the same heap used by the child's
J3D allocations and it survives for the required parent-resource lifetime.

The hook accepts only owner `pumpkin`, the exact 17,824-byte J3D header and
source FNV-1a `9b2ddb5ecbd95421`. Mutation then requires the exact rebuilt
topology hash, 306 positions, 1,818 normal slots, no envelopes, and S16 XYZ
normals with six-byte stride and 15 fractional bits. Duplicate source replacement
is rejected by pointer lookup. The nested owner stack is removed by the matching
resource-load post hook, including non-target and failed loads, and is cleared on
mod shutdown.

Live validation reported 495 triangles, zero degenerate triangles, zero normal
index conflicts, zero ambiguous faces, finite output, and 1,066 changed normals.
Twelve instances shared one rebuilt model. Default-off runtime performed no
rebuild or mutation. Matched natural captures showed no new faceting, silhouette
change, texture corruption, material-boundary leakage, or hard-edge loss.
Graceful shutdown restored the 10,908-byte backup before mod unload. Review found
no buffer overrun, invalid index, unsupported-format write, stale owner, double
processing, explicit-free mismatch, or regression in existing allowlisted paths.
