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

This is a candidate host fix, not an enabled MidnaFX feature. CPU-skinned
normal mutation remains off until the patch is integrated into a supported
host and tested across more model layouts.

## Optimized draw replay (2026-09-18)

The candidate patch was applied temporarily to the pinned host. A test-only
hook in `J3DModel::entryModelData` loaded each local BMD, created a model,
called `setSkinDeform`, and ran one `calc`. The hook was removed after testing.
The host launched with the source-matched DVD image, D3D11, and developer
logging. The reader saw Aurora `DrawIndexed` commands in every matrix group;
the raw-draw workaround from the earlier check was not used.

| Sample | Vertices mapped | Optimized indexed draws | `setSkinDeform` | `calc` |
| --- | ---: | ---: | ---: | --- |
| Original `bh.bmd` | 2,847 | 9 | 0 | Completed |
| Reindexed `bh.bmd` | 5,817 | 9 | 0 | Completed |
| Original with first draw opcode corrupted to `0xff` | Rejected | N/A | 6 | Skipped |

Aurora reported `unknown opcode (opcode 0xFF at offset 0)` for the negative
case, and the patched mapper returned `Invalid or empty CPU skin display list`
before `setSkinDeform` returned 6. Logs are in the ignored local
`build/runtime-smoke/stdout-cpu-optimized-{original,reindexed,malformed}.log`.
All temporary host edits were removed, its checkout is clean, and the normal
host was rebuilt. This validates the optimized representation and failure
propagation for the pot sample. It does not yet prove animated CPU skinning,
other model descriptor layouts, or the fast-skin display-list path.

## Animated replay and fast-skin boundary (2026-09-18)

The same patched host loaded `bh_attack.bck` from `B_bh.arc` and attached its
joint animation to each isolated pot model. `bh.bmd` has 25 joints, 12
envelopes, and model-data flag `0x2` (without the `0x100` fast-skin bit). Frames 0, 10, and 20 (of 40)
completed `calc` for both original and reindexed models. Joint 1 matrix
element `[0][0]` changed `0.6098 → 0.6687 → 0.5485`; hashes of the full
transformed normal buffer changed on every frame:

| Model | Frame 0 | Frame 10 | Frame 20 |
| --- | --- | --- | --- |
| Original | `65d5f8b46306f12b` | `6246925b93bc04f3` | `1f3b7d55244430ec` |
| Reindexed | `b3620fe838bc82ed` | `4cad0629bf54d7fa` | `02c3d637403fd286` |

These hashes show animated CPU deformation executed and produced changing
buffers. They do not establish visual normal quality or safe ownership for
production archive mutation. Local ignored logs are
`build/runtime-smoke/stdout-cpu-animated-{original,reindexed}.log`.

Review of `changeFastSkinDL` found another raw-strip parser that rewrites
display lists in place. Aurora may already have converted those lists to
indexed draws on PC, so the candidate patch now rejects `0x100` fast-skin
models before that rewrite and returns error 6. An isolated pot load with
flag `0x102` confirmed that return and emitted
`CPU skinning fast display lists are unsupported on PC`; see
`build/runtime-smoke/stdout-cpu-animated-fast.log`. The boss-door caller now
checks every nonzero `setSkinDeform` result. A full fast-skin implementation
would need an optimized-list-aware rewrite and separate validation.
