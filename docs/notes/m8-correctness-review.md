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
