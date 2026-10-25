# Enhanced Link shading

MidnaFX 1.4 exposes **Enhanced Link shading** under Basic controls and
enables it in the Remaster profile. The first supported model is the original
casual outfit, `Bmdl.arc/bl.bmd`, from the USA GameCube disc. Other outfits,
characters and replacement models remain unchanged. This is a normal-shading
improvement, not added geometry, a new silhouette or material normal mapping.

The planner retains 75% of each authored normal direction and blends 25% toward
the angle-weighted geometric average of a compatible smoothing group. Material
boundaries, authored normal splits and the face-angle limit constrain grouping.
If an original normal index is shared by incompatible proposals, that index
retains its exact original value. Faces whose orientation cannot be inferred
also retain every referenced normal index, including uses on other faces. These
faces do not contribute to smoothing other regions. Malformed input, unsupported
representations and unrelated topology ambiguity still reject the model.

No mesh indices, skin weights, animation, UVs or resource sizes are rewritten.
The normal array is backed up before the first write. Disabling the control,
selecting Vanilla, unloading the module or deleting the archive restores the
original bytes before releasing ownership. Enabling the control locates a
resident supported model once, without fetching resources or taking references.
Ordinary frames do not repeatedly scan or smooth the model.
Activation occurs on the next game update; a developer console simulation rate
of zero delays activation until simulation resumes.

Resident qualification requires the archive and filename, resource size, model
counts and the canonical hash of the complete native-loaded resource. Only the
three implicit padding bytes in each converted vertex-format record are
normalized for hashing. All semantic bytes remain part of the identity check.
The model must belong to a native model archive node before its resource pointer
is treated as `J3DModelData`.

The original source is 165,088 bytes with raw FNV-1a `b3c38a9a41e5b793`.
Its native canonical identity is `c945defef88edfc9`: 2,301 positions, 2,282 normals,
8 shapes, 133 envelopes and 3,617 triangles. The independently decoded corner
hash is `fefc475b8f9a6309`. No game assets are distributed with this feature.

Broader outfit support and artistic review remain separate work. macOS retains
best-effort support; native gameplay and Metal execution are unverified.

Windows D3D12 compares a frozen Ordon scene, exercises direct restoration,
reenable and native module reload, and closes cleanly. See the
[images and artifact-specific results](evidence/character-shading-2026-10-25/README.md).
