# Automatic rigid shading: 2026-10-29

## Artifact and native setup

The final local MidnaFX 1.15.0 Release package has SHA-256
`61637925EC5620AFBA48A258C579CF6CCB0ACEA7C111712E3C11D62332D37102`.
The native Windows host has SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The [launch script](launch-native.ps1) runs the existing exact-name host with an
isolated user directory, no fixture mod, D3D12 debug logging and Ordon
`F_SP103,0,27,0`. Model shading starts disabled. Geometry/topology diagnostics are
enabled, Link shading and other scene effects are disabled, and motion clarity
remains enabled. External replacement textures are disabled.

Session receipts identify each tested binary separately. The [strict baseline
receipt](resident-off/session.json) and [partial-planner receipt](partial-resident/session.json)
precede the final alias-history guard and final help text. They are development
comparisons within this slice, not benchmarks of the published 1.14 package.
The [final receipt](final-resident/session.json) identifies the final source build.
No host or game payload is redistributed here.

## Native coverage and ownership

The [assertion script](analyze.py) reads the retained logs and receipts to produce
the [summary](summary.json). All three sessions close normally with exit zero
and report all mods unloaded.

- The [strict baseline log](resident-off/native.log) records 115 examined models,
  three applied arrays and three exact restorations after UI disable.
- The [partial-planner log](partial-resident/native.log) records 115 examined,
  twenty applied arrays and twenty exact restorations after UI disable.
- The [final log](final-resident/native.log) records 107 examined, twenty applied
  and 87 skipped on each of two UI activations. UI disable restores all twenty
  arrays exactly. A second activation reapplies them. Native mod reload restores
  all twenty exactly again, then the new module session applies zero cached
  models because their complete load history is unavailable. The two restoration
  cycles total 32,592 bytes. No ownership-conflict warning occurs.

All twenty identities have `knownGood=no`: the increase does not add asset
allowlists. These are loaded normal arrays across item, prop, door, accessory and
insect resource archives. Loaded resources need not be instantiated or visible in
the current scene. Counts do not establish twenty visible improved objects,
whole-game coverage, or an artistic quality score. The lower final examined
count follows the stricter source/history gates; it does not represent a matched
scene performance comparison.

The [final enabled](final-enabled.jpg), [disabled](final-disabled.jpg) and
[reloaded](final-reloaded.jpg) screenshots show native control integration.
Earlier [world](native-original.jpg), [enabled](control-enabled.jpg) and
[disabled](control-disabled.jpg) captures belong to the partial-planner binary.
Time, actors and environment are not frozen. These captures are context, not a
matched image-quality comparison. No movement/camera route or forced native
deformation-alias test is claimed.

## Portable proof and limits

The Release ALL build and [seven focused checks](focused-tests.log) pass:
`rigid_shading`, `character_identity`, `topology`, `smoothing`, `qualification`,
`bmd_rebuild` and `package_contract`. New checks exercise:

- Rigid envelope/joint/matrix, billboard/bump and shape/material count rejection.
- Original direct and pooled vertex matrix-zero enforcement, including a
  nonzero index late in a pooled record. Existing decoder behavior is unchanged
  when the new requirement is disabled.
- Global preservation of normal indices at duplicated position identities and
  overlapping authored groups, safe changes elsewhere, reversed input order,
  all-protected no-op behavior and unchanged strict rejection defaults.
- Six- and twelve-byte per-normal ownership restoration, conflicting external
  records, invalid sizes, canaries, and raw/range source-alias matching including
  adjacency, nulls, empty spans and address overflow.

Source review verifies that both native deformation attachments can retain or
transform raw arrays. Guards restore matching rigid overrides before attachment
and exclude that source generation, including model-data aliases sharing raw,
normal or position storage. Native initialization confirms both hooks register.
Portable alias tests and source inspection do not establish every game's runtime
deformation path or compatibility with arbitrary third-party writers.

The resident sweep checks a two-millisecond deadline between models; one model
is indivisible. The partial run includes an `Alink/wl_change.bmd` classification
of 5.4 ms plus a 2.7 ms plan. This is not a hard frame-time cap. Completed sweeps
stop recurring topology scans. Original/applied normal backups share a 16 MiB
budget, while classification has separate bounded working storage.

The feature changes qualified normal shading only; it adds no GPU pass and does
not improve silhouettes or create new content. Broad art, motion, gameplay
deformation and performance quality remain unqualified. Enhanced model shading
stays opt-in and Remaster does not enable it. After mod reload, restart the game
for complete automatic coverage; cached scene resets may retain old resources.
macOS remains best effort through native package builds and portable checks.
Native macOS gameplay and Metal execution are unavailable on this host.
