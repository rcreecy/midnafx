# Airborne-source reuse and integration — November 12, 2026

**CPU-06 retained.** Native measurements rank airborne anchor selection ahead of
resource discovery. The pilot measured discovery at **11.0 us p50** and airborne
selection at **161.15 us**, returning six anchors after scanning cached vegetation
and sorting a temporary list. The discovery algorithm remains unchanged.
[Pilot capture](pilot/manifest.json), [summary](pilot-summary.json).

## Implementation and correctness

The source cache stores at most 64 copied point/forest records and scalar keys.
It compares all projection-matrix and eye float bit patterns, terrain revision
and requested output count. A hit copies the exact ordered result of the original
query. Any change executes the original scan and `partial_sort`; requested counts
larger than capacity use the original path without truncation. Including requested
count preserves tie behavior, which can depend on the partial-sort prefix size.
Unused output entries remain untouched. Cover clear resets the cache; model/room
changes advance the existing terrain revision. No game-resource pointer is held.

Particle time, wind, weather/day/night response, sprite generation, lighting and
GPU uploads still execute independently every frame. No particle count, source
placement, shader, quality control or feature toggle changes. Storage is bounded
to roughly 1.1 KiB on x64, with no heap allocation on hits. Moving queries still
use the baseline algorithm; their CPU speed is not established here.

The existing bounded diagnostics capture now adds separate discovery and source
selection spans, with source counts, reuse flags and ordered source hashes.
Source hashing runs outside the measured query interval. Diagnostics off takes
no new clock readings. These fields do not redefine the five earlier grass spans.

## Fresh native comparison

Three reversed pairs use the established Hyrule Field `F_SP121,6,2,4` scenario:
i7-10700/UHD 630, D3D12, 1216x896 scale 1, original textures, same starting config,
Balanced power policy, isolated user/mod directories, 55-second runs, 384 recorded
stages, declared warmup of 288 and 96 ready samples per run. All six exit normally
with code 0. Every candidate sample reuses six anchors. All three settled ordered
anchor proofs match the references exactly.
[Raw captures](paired/manifest.json), [complete analysis](paired-summary.json).

| Pair/order | Selection p50 before → after (us) | p95 before → after (us) | p99 before → after (us) |
|---|---:|---:|---:|
| 1 / AB | 162.50 → 0.20 | 205.78 → 0.50 | 301.84 → 0.60 |
| 2 / BA | 162.85 → 0.20 | 204.93 → 0.53 | 234.64 → 0.71 |
| 3 / AB | 168.70 → 0.20 | 196.43 → 0.40 | 264.85 → 0.61 |

The median paired p50 ratio is about 0.00123, or **99.9% lower measured selection
wall time** in the settled-camera case. The optimized timer is near clock-read
resolution; the exact speedup multiple is not a hardware-independent claim.
Discovery remains measured separately and is not optimized. No GPU duration,
native total-frame percentile or FPS gain is inferred.

## Qualification before merge

The user explicitly requested merge to main and origin, then clarified that the
complete current MidnaFX working tree should be included, including preexisting
unreleased features and optional patch artifacts. This authorizes publication of
those existing files; the optimization work does not edit either upstream checkout
or alter their patch contents. [Starting hashes](workspace-baseline.json),
[preservation checks](preservation.json), [integration diff](implementation.diff)
and [source/package identity](delivery.json) separate the work.

The two earlier failures were investigated before integration:

- `surface_maps_pixels` passed an unsupported `--surface-maps-only` option to
  `aa_pixels`. No such fixture or dispatch exists. It duplicated the entire AA
  suite under a shorter timeout. That false registration is removed; the real
  AA suite still runs. The executable now rejects unsupported/extra arguments
  before GPU setup, and `aa_cli` verifies the failure code and message.
- `surface_map_contract` expected two new-file payloads absent from the optional
  patch, which also contains no references to those headers or their namespace.
  It now reports **not applicable / skip 77** when the experiment is entirely
  absent. A partial, referenced-but-missing, duplicate or stale payload still
  fails. Synthetic parser fixtures test those branches. The existing standalone
  cache tests and shader compilation remain; they do not prove host integration.
  No host payload is added or repaired to change that status.

The final full Windows Release run has **48 passed, 1 explicitly skipped, zero
failed**, across 49 registered tests, in 136.71 seconds. The actual main D3D12
pixel suite, ambient/bloom pixels, immutable-buffer lifecycle, shader compilation,
source-cache tests and package contract all execute successfully.
[Full CTest receipt](full-tests.xml), [log](full-tests.log), [build](full-build.log).
The absent host surface-map integration remains unqualified. Earlier failed
receipts are preserved and are not retroactively relabeled successful.

## Limits and disposition

Retained for exact-input queries: reproducible removal of redundant scans and
temporary-list allocation, exact ordered results, bounded value ownership and
invalidation proof justify this small cache. Broader camera motion, traversal,
resource replacement and hot-reload coverage remain open. Native runs retain the
existing missing-card and post-unload mapping/device-destroyed warnings. There
is no frozen full-frame image comparison or Metal execution in this round.

The current Windows package matches the native candidate. Performance capture
and the source-cache code are unchanged by the subsequent qualification test
fixes. GPU capture and unmeasured moving/cold scenarios should rank additional
engineering work before more steady-state CPU caching. No release tag or GitHub
release is requested or created by the main/origin integration.
