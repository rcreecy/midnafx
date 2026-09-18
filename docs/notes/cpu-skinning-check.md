# CPU normal-index check (2026-09-18)

The pinned Dusklight host is `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
This was an isolated Windows/D3D11 source-matched test using temporary edits
under the ignored `upstream/dusklight` checkout. The edits were removed and
the ordinary host rebuilt afterward. No game archive or shipped MidnaFX code
was changed.

`B_bh.arc/bh.bmd` was chosen because all three shapes have direct PNMTX and
16-bit indexed position, normal, and TEX0 attributes, matching the assumptions
in `J3DSkinDeform::initMtxIndexArray`. The independent raw BMD decoder found
1,939 triangles, 1,200 original normal entries, and nine shape/matrix groups.
The full in-memory BMD rewrite produced 5,817 unique triangle-corner normal
indices, 7,017 actual normal entries, and 7,018 padded array entries. Its
remapped corner hash matched the original `b3e7eb572d471d52`. Local ignored
sample files and logs are under `build/runtime-smoke/bh-*.bmd` and
`stderr-cpu-*.log`.

The unmodified PC draw path returned success from `initMtxIndexArray` for
both original and rewritten samples but visited **zero** corners and mapped
zero normals. `J3DShapeDraw` had copied and Aurora-optimized the draws; the
CPU mapper accepts only raw `GX_TRIANGLEFAN`/`GX_TRIANGLESTRIP` commands and
stops at the first optimized command. This is a false-success result, not a
CPU skinning pass. `J3DModel::setSkinDeform` also discards the mapper's return
value, so a future error would not fail the setup at that call site.

Temporarily preserving raw draws for only the diagnostic model exposed a
second problem. This PC build takes the `#if PLATFORM_GCN` branch in the CPU
mapper because the selected game `VERSION` is GCN USA. It reads the first
big-endian strip count `00 15` as 5,376 instead of 21. A test-only bounds guard
caught an out-of-range normal index before a write. The unguarded diagnostic
crashed in `initMtxIndexArray`; the crash is reproducible on the **original**
resource, so it is not caused by normal reindexing.

With raw draws confined to the diagnostic model and the PC branch used for
draw counts and vertex stepping, both models completed CPU setup and one
`J3DModel::calc()` call:

| Sample | Draw corners visited | Distinct normal indices mapped | Matrix conflicts | Setup/calculation |
| --- | ---: | ---: | ---: | --- |
| Original `bh.bmd` | 2,847 | 1,196 | 0 | `setSkinDeform=0`; non-null normal buffer |
| Reindexed `bh.bmd` | 5,817 | 5,817 | 0 | `setSkinDeform=0`; non-null normal buffer |

This establishes that a per-corner normal split can pass the CPU mapping and
calculation routines on one compatible sample **after** those two host-path
corrections. It does not validate animated CPU skinning in the unmodified host,
normal quality, or a production buffer lifetime. Link's sampled instance did
not invoke `setSkinDeform` at all. CPU-skinned resource mutation remains
default off until Dusklight has a tested raw/optimized draw interpretation,
correct PC endian/step handling, and a fail-closed setup API; then repeat the
test in an animated scene.

## Proposed PC host correction

`patches/dusklight-cpu-skinning-pc.patch` is a reviewable patch against the
pinned host revision. It uses Aurora's bounded display-list reader on PC, so
both plain draws and optimized indexed draws contribute vertex-to-matrix
assignments. It checks descriptor and index bounds, rejects an empty or
malformed draw group, and propagates setup errors from `setSkinDeform`. The
console path is untouched. The patched Windows host compiled and linked, and
the patch applied cleanly to a restored pinned checkout. The ordinary host
was then rebuilt from clean upstream sources.

This is a candidate host fix, not an enabled MidnaFX feature. It still needs
a targeted runtime replay of original and rewritten `bh.bmd` through the
optimized draw path, a negative malformed-list test, and an animated CPU skin
sample before CPU-skinned normal mutation can be enabled. In particular,
`changeFastSkinDL` also edits display lists with a raw GX parser and needs a
separate review for any model using the fast-skin flag.
