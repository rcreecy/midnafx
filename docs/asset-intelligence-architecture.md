# Asset intelligence architecture

## Scope and implementation

The diagnostic observes newly loaded `J3D2bmd3` resources. It does not modify resources, hook arguments, material state, texture registration, or render submissions. `asset_diagnostics` defaults to false and appears in the developer qualification section as “Asset identity diagnostics (new loads only).” Existing rendering defaults are unchanged.

The end-to-end path is implemented in:

- [geometry_probe.cpp](../src/game/geometry_probe.cpp): reuse pre/post resource load, archive delete, model creation, initialization and shutdown.
- [asset_probe.cpp](../src/game/asset_probe.cpp): bounded session bindings and schema-1 JSON events in the existing log.
- [asset_identity.hpp](../src/game/asset_identity.hpp): concrete TEV texture-array capacities, including unsupported tags.
- [asset-intelligence.py](../tools/asset-intelligence.py): reuse RARC/Yaz0 reading, parse original MAT3/TEX1 metadata, compute source keys, join runtime observations and inventory replacement candidates.
- [settings.cpp](../src/ui/settings.cpp): one opt-in developer toggle.

## Identity and ownership

| Identity | Source / owner | Stability and confidence |
|---|---|---|
| Resource | Archive basename, resource name, full original BMD FNV-1a-64; offline SHA-256 and byte count | Stable across the tested launches/reload. FNV is a correlation key, not a collision-proof identifier. Archive basenames do not prove disc directory provenance. |
| Model | Resource identity plus temporary loaded `J3DModelData*` binding | Resource identity persists; pointer is session-local and never exported. |
| Material | Model identity plus original MAT3 index/name | Both pumpkin material indices and initial live texture slots are established. Runtime animation can subsequently change bindings. |
| Texture | Model texture-table index/name plus dimensions, GX format, XXH64 base mip and optional TLUT hash | Identical image data may occur in many table entries, materials and models. Entries are deliberately not deduplicated. |
| Creation | Shared model identity plus an observation ordinal | Historical successful model-creation event, not a persistent actor ID, live-instance count or rendered-object identifier. Ordinals restart when the archive binding is recreated. |
| Replacement | Exact-key candidate relative path | Candidate only. TextureService v1 cannot reveal the selected replacement, registration precedence outcome or streaming completion. |
| Rendered object | None | Unavailable through the audited public service. A screenshot cannot prove a material-to-submission join. |

The runtime hashes archive data before the native loader processes it. The offline parser joins only the exact resource fingerprint. It computes material and texture metadata from the matching original file; this does **not** mean the runtime has captured every GPU texture binding. Runtime slots outside a concrete TEV block's capacity are `null` (unobserved), while original MAT3 unused slots are `65535`. The join checks every observed slot and does not treat `null` as a match claim.

Original fingerprints, table indices and source-key calculations depend on asset bytes, not the rendering backend. Native bindings still depend on the pinned host ABI and its resource lifecycle. Cross-backend identity stability is a design property of those calculations, not a live Metal result; this experiment only exercised D3D12 gameplay. Animated materials and texture matrices are not sampled after initial load.

## Lifecycle and bounds

Bindings contain copied identity strings and non-owning owner/model addresses. Archive deletion erases all bindings for that owner before the game frees them. No instance address is retained. Mod initialize/shutdown clears bindings and releases vector capacity. Turning diagnostics off stops new discovery/material/creation work; delete cleanup still runs, so retained bindings cannot outlive archive ownership. Turning it on does not retrospectively scan already loaded models.

There is no new frame callback or per-frame enumeration. Work occurs at resource load, model creation and archive deletion. Disabled callbacks return before scanning/hashing; an empty delete cleanup and scalar checks remain at the existing hook boundaries.

Limits are 128 concurrent resource records, 8 MiB per BMD, 256 materials/textures in the offline parser, 64 BMD blocks, eight material texture slots, and 4,096 runtime events per mod lifetime. The final event reports truncation. Resource saturation reports incomplete discovery; it was reached in Ordon, so this experiment is not a complete scene inventory. Unsupported formats are omitted or reported; BDL, BMD2, runtime-generated assets and arbitrary archive variants are not qualified. Native archive tables are trusted only to the same extent as the host loader; the probe is not a sandbox for hostile native memory.

Exceptions in the observer are isolated from the game operation. Offline malformed archives/models return structured failures and a nonzero exit code. Invalid or truncated runtime JSON fails the export; use a closed/flushed log. A failed export must never be treated as a successful partial proof.

## Structured output and normalization

Runtime events carry `schema: 1`, event kind, copied resource identity and provenance where relevant. They contain no raw pointer or game bytes. Timing events are session measurements. The offline graph adds declared MidnaFX revision, pinned Dusklight revision, platform, backend, scene, resource SHA-256, original materials/textures, creation/load/delete counts, runtime limitations and explicit unavailable rendering/replacement fields.

Revision/platform/backend/scene arguments are operator-supplied session context, not auto-attested by the host. Capture the package hash and launcher manifest alongside the log, as done in [the evidence summary](evidence/asset-intelligence-2026-10-19/summary.json). Models, keys, candidate paths and observed material sets are sorted. Compare original identity fields across runs; timings, creation counts, unload counts and observation order intentionally differ with session activity.

Example, using a legally available local archive and a completed runtime log:

```powershell
python tools/asset-intelligence.py path/to/pumpkin.arc `
  --midnafx-revision <tested-commit> --platform windows-amd64 `
  --backend d3d12 --scene F_SP103,0,27,0 `
  --runtime-log path/to/stdout.log > pumpkin-graph.json
```

Add `--replacement-dir path/to/texture_replacements` to inventory exact DDS/PNG source-key filenames. This does not decode image payloads or reproduce wildcard, mipmap, pointer-registration, priority or streaming rules. A missing directory fails explicitly. No private texture-cache traversal or shader replacement is introduced.

## Smallest future rendering extension

First add an optional, versioned host observer at the J3D packet submission boundary. It should copy a frame-local draw token, model/material/shape correlation token and original texture source keys into a bounded diagnostic payload. Associate that payload with the encoded native draw, without calling game objects from the render worker. Invalidate tokens at archive destruction and frame retirement; never expose native pointers as durable identity. Filtering to one known resource is essential for overhead control.

A separate read-only TextureService query should return the resolved source key, selected replacement registration/generation, provider and load status. Both interfaces need explicit ownership and threading contracts. Neither exists in this implementation. Validate correlation before adding any material-specific shading control; identifying a material alone cannot change its shader safely.
