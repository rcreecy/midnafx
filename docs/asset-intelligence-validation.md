# Asset intelligence validation

Date: 2026-10-19. The proof is limited to the naturally instantiated Ordon pumpkin on Windows D3D12. The diagnostics are experimental, disabled by default, and are not a new release.

## Outcome matrix

| Objective | Outcome | Evidence and limit |
|---|---|---|
| A: identify original pumpkin resource | **PASS** | 17,824 bytes; FNV-1a-64 `9b2ddb5ecbd95421`; original SHA-256 in the exported graph. |
| A: material and texture relationships | **PASS** | Two original materials; live initial slots agree; four original TEX1 entries share two keys. Unobserved live slots remain null. |
| A: shared model creation | **PASS** | Twelve successful creation observations per Ordon load. This is not a live actor census. |
| A: repeated launches and archive lifecycle | **PASS** | Session b loads Ordon, leaves for `F_SP127`, then returns. Two identical resource loads, one delete and 24 creation events. Session c independently repeats the same identities with 12 creations. |
| B: original identity with Henriko pack enabled | **PASS** | Session e registers 1,759 replacements and preserves all original graph fields and both live material mappings; 12 creations. |
| B: associate actual replacement files | **PASS** | Each original texture entry has one exact-key DDS candidate; two distinct files, with inspected headers and hashes. |
| B: attest selected replacement per GPU submission | **BLOCKED** | TextureService v1 exposes no resolved-selection query. Candidate registration and screenshots do not prove a per-draw selection. |
| C: native material to GPU draw | **BLOCKED** | GfxService v2 custom draws/stage callbacks lack native J3D submission identity. No speculative host patch added. |
| Mod reload and shutdown | **PASS** | Native Reload action in session c logs shutdown, reload and reinitialization, then normal host shutdown. No crash observed. Existing resources are not rescanned. |
| Disabled diagnostic behavior | **PASS** | Session d reaches the same scene, produces zero AssetGraph events and shuts down normally. Source checks show no added frame work. |
| Malformed metadata and failure isolation | **PASS** (offline) | Bounded fixtures, invalid slots/indices/schema, missing directory, malformed archive CLI output and unchanged input bytes. Hostile native archive injection was not tested. |
| Animated materials, other assets/backends | **NOT TESTED** | No generalized material animation, instance visibility or whole-game compatibility claim. |
| Exact pixel equality / total instrumentation cost | **NOT TESTED** | Read-only code and normal rendered scenes were checked; no deterministic pixel A/B or complete allocation/CPU profiler was used. |

## Durable evidence

- [Summary](evidence/asset-intelligence-2026-10-19/summary.json): package/log SHA-256, event counts, load timings, record bounds, reload/shutdown observations and comparison results.
- [Vanilla round-trip graph](evidence/asset-intelligence-2026-10-19/pumpkin-graph-b.json) and [pumpkin events](evidence/asset-intelligence-2026-10-19/asset-vanilla-b.jsonl).
- [Independent vanilla graph](evidence/asset-intelligence-2026-10-19/pumpkin-graph-c.json) and [pumpkin events](evidence/asset-intelligence-2026-10-19/asset-vanilla-c.jsonl).
- [Pack-enabled graph](evidence/asset-intelligence-2026-10-19/pumpkin-graph-e.json), [pumpkin events](evidence/asset-intelligence-2026-10-19/asset-pack-e.jsonl), and [pack manifest](evidence/asset-intelligence-2026-10-19/pack-manifest.json).

Full local logs, launch manifests, isolated settings and packages remain under `build/m13-aa-runtime/asset-vanilla-b`, `asset-vanilla-c`, `asset-disabled-d`, and `asset-pack-e`. These contain debug output unrelated to this proof and are ignored by Git. Local captures are [Ordon after return](../build/m13-aa-runtime/asset-vanilla-b/ordon-return.jpg), [vanilla](../build/m13-aa-runtime/asset-vanilla-c/ordon.jpg), [disabled](../build/m13-aa-runtime/asset-disabled-d/ordon.jpg), and [pack enabled](../build/m13-aa-runtime/asset-pack-e/ordon.jpg). Captures establish rendered scene context, not GPU attribution or pixel equivalence.

Sessions c/d/e use the same package SHA-256 `a3b010759ca76a8bd96577b98c5074604587a50e1a5f60353f90e001bd34f861`. Session b predates the added memory estimate/event-limit hardening, but uses the corrected TEV-capacity logic. An earlier exploratory session a used an incorrect eight-slot live TEV read and is excluded from accepted material evidence. The bug was fixed by reading only each concrete subclass's capacity and covered by a native regression test.

The standalone graph context names the tested working tree as `a72340e+asset-probe-working-tree`; package hashes identify the actual runtime artifact. Later offline parser error handling/test additions do not change the tested DLL. Runtime event subsets contain copied metadata only. No disc archive or texture payload is distributed.

## Pumpkin graph

`pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421` has SHA-256 `dc05ad71242f45680a2a6b68add972a2a7877f52e74a9206e5041439de20c70c`.

| Material | Initial original texture slot 0 | Original source key | Pack candidate header |
|---|---|---|---|
| 0, `pumpkin_happa_m` | TEX1 index 3, `pumpkin_happa` | 64×64, GX format 14, `630d5c7bc606585d` | 2048×2048 DDS, DX10 header |
| 1, `pumpkin_m` | TEX1 index 2, `pumpkin` | 64×64, GX format 14, `000e342106676e31` | 1024×1024 DDS, DX10 header |

TEX1 indices 0/2 share the pumpkin key; indices 1/3 share the leaf key. No palette is used by this model. The live TVB2 blocks expose two slots: the second is unused (`65535`); six further slots are unobserved (`null`). The original MAT3 table has seven unused slots after slot 0. The twelve creation observations all resolve to the same resource, not twelve distinct texture identities.

Comparison asserted equality of resource ID, archive/resource names, byte count, SHA-256, all original materials and all original texture keys. Replacement candidate fields and lifecycle observation counts were excluded from that stable comparison. The pack's archive SHA-256 is recorded in the manifest. The filename mapping was checked against actual pack files and pinned Aurora parsing, not inferred from the marketing name.

## Automated verification

The existing Release build/package completed successfully. Full Windows CTest passed **18/18** in **67.29 seconds**, including shader compilation, AA pixels, topology, geometry qualification and package contract. After final Python failure-reporting and recorded-evidence assertions, both asset CTest entries passed again (**2/2**, 0.41 seconds), with eight Python test cases.

[Native asset tests](../tests/asset_identity.cpp) verify every supported TEV subclass capacity and unsupported tags. [Python tests](../tests/asset_intelligence.py) cover repeated read-only model parsing, material slots, malformed bounds, palette bounds, shared creation/delete observations, schema rejection, actual source-key filename matching fixtures, missing directory and structured CLI failures. XXH64 vectors across 31/32/33/63/64/127/2048 bytes were independently generated using the pinned upstream `xxhash.h`; the parser agrees. Palette coverage is synthetic C4 evidence, not live qualification of all indexed formats.

Hosted [CI run 39322759033](https://github.com/rcreecy/midnafx/actions/runs/39322759033) passed at implementation commit `dcdcd1d`: Windows **18/18** in 46.88 seconds and macOS Intel **18/18** in 33.89 seconds. Three additional Metal AA pixel repeats passed (17.04, 13.39 and 10.83 seconds). Both native packages were produced; the release job was skipped.

Logs: local `build/asset-build.log`, `build/asset-tests-final.log`, `build/asset-ci.log`. CI uses synthetic assets and metadata-only recorded evidence; it does not need the game or the pack. A native macOS gameplay session is **NOT TESTED**. Documentation-only clarification after `dcdcd1d` does not change the tested implementation.

## Overhead and safety

The memory metric estimates retained vector storage plus identity string capacity. Sessions c/e peak at **16,320 bytes** for **128 records**. It excludes allocator bookkeeping, transient strings, host logging buffers and existing geometry infrastructure. Session b did not record this metric; null means unavailable, not zero. Disabled session d retains no discovered records.

Measured pre-load observer work in session c totals **7.014 ms** across 55 callbacks, maximum **1.287 ms**. Pack session e totals **6.817 ms**, maximum **1.293 ms**. These timers include scanning/hashing and resource event logging but exclude post-load material/creation callbacks and the cost event itself. The longer round-trip session b totals 94.203 ms, maximum 31.236 ms; environment contention and debug logging make these observations unsuitable as a controlled benchmark. No FPS or total overhead claim follows from them.

All enabled sessions hit the 128-record discovery bound. Pumpkin was observed before saturation and again after unload/reload freed slots. Complete scene coverage is explicitly false. Logging has a 4,096-event ceiling; this ceiling was not reached in accepted sessions. The probe retains no game-owned instance pointers and cleans owner/model lookup keys on archive deletion and mod reset. Error paths do not alter hook return values or resources.

## Reproduction and remaining work

Use the pinned host and local disc, an isolated user/mod directory, Ordon `F_SP103,0,27,0`, and `mod.com_midnafx_midnafx.asset_diagnostics=true`. Keep geometry mutation/smoothing experiments off. Export the graph using the [documented command](asset-intelligence-architecture.md). For the lifecycle experiment, run native console `warp F_SP127 0 0 0`, then `warp F_SP103 27 0 0`. Close the host before reading redirected logs.

Repeat with diagnostics false, then with the actual pack extracted under the isolated user directory's `texture_replacements` folder and `game.enableTextureReplacements=true`. Confirm the host registration count and retain package/log hashes. Do not copy the pack into Git. The next milestone is the filtered native draw/texture-selection observer described in the [roadmap](asset-aware-remaster-roadmap.md), not a new shading effect.
