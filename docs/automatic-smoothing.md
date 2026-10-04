# Automatic model shading qualification

## Starting audit (2026-10-03)

Work starts at `69aedbfd44bd9d3511d0df5985c842df9b3bc540`, branch
`codex/m12-water`, with a clean working tree. The common ancestor with main's
`3f61159c35fd1ecd8ba3f87e39bf7bbf8400121c` is release preparation
`bc6fcbd4f6aae3579afef234a3deee77177fb8e5`. This checkout contains three later
water commits but not the main README update. Water work is excluded.
The published v1.0.0 release and main/release GitHub Actions runs succeeded.
Existing Windows test binaries pass 13/13. A fresh baseline build fails in
the pre-existing water translation unit because actor headers lack animation
type declarations. Cached test success is not a fresh build result.

The SDK remains at `edf42c6a7202647b56dd2fcdef02d17671bc814b`, with local
CameraService modifications in five files. Historical M7/M8, camera, DOF,
render ownership, v1 readiness and release correctness notes were inspected.
Their engineering completion does not establish broad product qualification.

## Existing geometry contract

- `dRes_info_c::loadResource` post-hook visits loaded J3D model resources before
  actor instance creation. Its pre-hook and `J3DModelLoaderDataBase::load` hook
  optionally rebuild exact pot, Link, beehive and pumpkin resources.
- Rebuild identity includes archive/file, original size and FNV-1a. Mutation
  additionally requires rebuilt ordered corner hash, position/normal counts,
  envelope count, encoding, stride and fraction. The rock uses an original
  resource fingerprint instead of a rebuild. Fingerprints establish the tested
  representation and authorize the separately validated index rewrite.
- The bounded topology decoder handles original GX strips/fans/quads/triangles
  and Aurora indexed draws. It retains position/normal indices, shape, material
  and matrix-group identity. Direct positions/normals and NBT3 are unsupported.
- The planner groups corners by position identity, material, 55-degree face
  angle and 20-degree authored-normal split. It retains 75% authored direction.
  A shared normal index requiring directions more than one degree apart rejects
  the whole plan. It never duplicates indices. UV coordinates are not welded.
- Historical adjacency joins every face at a position, including disconnected
  fans. Automatic qualification must reject these ambiguous fans. Degenerate
  faces, ignored lines/points and nonfinite input cannot silently disappear from
  automatic safety decisions. Normal validation must precede normalization.
- Backup ownership is per model and archive. Shared actor instances reuse one
  mutation. Archive pre-delete, class-specific disable and shutdown restore
  original bytes before releasing bookkeeping. Reload restores old models;
  enabling again requires a subsequent resource load. Packed child replacements
  belong to the parent's current solid heap. No per-frame topology work exists.
- The UI exposes a static smoothing toggle, angle, topology/catalog diagnostics,
  and a hidden skinned toggle. Both mutation classes default off. Historical
  measurements range from 120 us for beehive to 2,444 us for Link analysis;
  Link rebuild adds 2,501 us. These are historical samples, not new benchmarks.

## Classifier policy

`SAFE` means a compatible rigid representation and an unambiguous, conflict-free
plan. It is structural permission, not visual approval. `AMBIGUOUS` covers
degenerates, non-manifold edges, disconnected fans, inconsistent winding,
duplicated position identities, missing materials, uncertain orientation and
normal-index conflicts. `UNSUPPORTED` covers malformed decode, unsupported
normal layout, missing lifecycle/bounds evidence, excessive work and unknown
skinning or matrix ownership. Every such result prevents writes.

Known-good exact fixtures retain their separately validated representation path,
including existing rebuilds, but cannot bypass finite input, bounds, encoding,
conflict or ambiguous-face checks. Known-bad evidence vetoes any plan. Unknown
models never receive index duplication. Unknown enveloped or multi-joint models
need a future matrix-palette/influence proof before automatic writes.

The automatic classifier uses edge incidence and connected vertex fans. It
does not weld coincident positions or guess UV seam intent. Material and authored
normal boundaries remain mandatory planner constraints. Classification and
planning run on resource load only; rejected models are cached until archive
deletion. Tracked vector capacity is reported separately from process-memory
peak; timings are CPU durations, never GPU timings.

## Required product evidence

Synthetic tests cannot replace the requested organic, architecture, hard-surface,
foliage, Link, second-character and enemy runtime corpus. Each runtime record
must include class/reason, mutation, counts, conflicts, timings, memory, visual
comparison and lifecycle outcome. Unknown skinned resources and any models
requiring index splitting remain a disclosed coverage blocker. Default-on
promotion requires representative visual and lifecycle proof on this classifier.
