# Guarded automatic model shading

MidnaFX 1.15 upgrades the existing **Enhanced model shading** Basic control.
It remains an independent opt-in; Remaster does not enable it automatically.
The generic path qualifies rigid model classes from native structure rather than
adding archive/file allowlists. It does not add geometry or change silhouettes.
The separate Enhanced Link shading control retains its existing supported-outfit
contract.

## Qualification and preservation

Automatic candidates require a bounded original BMD, zero envelope matrices, one
joint, one rigid draw matrix referencing joint zero, non-billboard/non-bump state,
supported opaque materials and single rigid shape-matrix groups. Original source
descriptors and draw records are read together; mutable optimized draw copies are
not used as topology authority. Every referenced position-matrix index must be
zero. Position/normal storage must remain inside the owned resource, and shared
normal storage is rejected.

The existing planner retains 75% authored normal direction and blends 25% toward
an angle-weighted geometric direction within compatible material and authored
normal boundaries. It never welds coincident position identities. Instead of
discarding an entire otherwise qualified model for coincident position IDs or
overlapping threshold groups, the new policy preserves every normal index used
at those uncertain vertices. Preservation applies to all uses of an index,
including uses in other regions. Conflicting proposals and uncertain face
orientations also keep their original normals. Remaining independently safe
regions can improve.

Malformed data, unsupported normal encoding, excessive adjacency, nonmanifold
edges, inconsistent winding, disconnected vertex fans and other structural
ambiguities still reject the model. Strict planner defaults remain unchanged;
the broader preservation policy is explicit and used only for automatic rigid
models. Existing validated resource rebuilds remain a separate legacy path.

## Activation and ownership

Enabling the control visits eligible resident object and stage archives without
loading resources or taking references. The sweep checks its two-millisecond
budget between models and resumes from its archive/directory/file cursor. One
model's classification is indivisible and can exceed that budget. Completed
sweeps perform no recurring topology scan. New archive loads retain their normal
qualification path.

Automatic mutation requires successful load history from the current module
session and both native deformation-attachment guards. A model-data generation
loaded before MidnaFX initialization cannot establish that history. After
reloading the mod, restart the game for automatic coverage. A genuine archive
unload/load also establishes a new generation, but a cached scene reset may
retain the old resources. The independent Link contract remains unchanged.

Before a native deformation or skin-deformation attachment can copy or transform
rigid source arrays, the guard restores any owned rigid normals and excludes that
model-data generation until archive deletion. Matching also covers actor-created
model-data aliases sharing the raw resource or normal/position byte ranges.
Null detachments do not clear the
exclusion. Loss of bounded history disables automatic mutation and restores
existing rigid overrides.

Backups store both original and applied normal bytes within a combined 16 MiB
budget. Disable, archive pre-delete and module unload restore each normal only
when its current bytes still match MidnaFX's applied bytes. An intervening writer's
record is preserved and reported. This cannot distinguish an external write of
the identical applied bytes. No asynchronous topology or raw instance pointer is
retained.

The generic path leaves positions, UVs, indices, skin weights, source sizes and
animation unchanged. Texture replacements contribute their existing appearance
without new metadata or redistributed payloads. No GPU pass or extra geometry is
added; qualification and backups do add bounded CPU work and memory.

See [the evidence ledger](evidence/rigid-shading-2026-10-29/README.md) for synthetic
policy/ownership tests, native coverage and remaining visual limitations.
Whole-game art, motion and performance quality remain unqualified. macOS remains
best effort through native builds and portable checks; gameplay/Metal execution
is unverified.
