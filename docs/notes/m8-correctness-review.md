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
