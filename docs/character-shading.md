# Enhanced Link shading

MidnaFX 1.4 exposes **Enhanced Link shading** under Basic controls and
enables it in the Remaster profile. Version 1.4.1 supports the original Ordon
Clothes (`Bmdl.arc/bl.bmd`) and Hero's Clothes (`Kmdl.arc/al.bmd`) from the USA
GameCube disc. Armor outfits, other characters and replacement models remain
unchanged. This is a normal-shading
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

For these fingerprinted characters, planning reads the original SHP1 draw lists
inside the source resource. Native optimized draw copies can change after model
creation: one observed standard-outfit triangle later repeats a position index,
although the source, decoded positions and descriptors remain identical. Analyzing
that mutable copy made re-enable reject an otherwise supported model. The source
adapter checks every block, shape remap, group, draw record and span, and the
decoded triangle count and corner hash must match the supported source. Native
draw copies remain untouched. Unknown models retain the existing analysis path.
Original draws are paired with their original converted descriptor records;
the renderer adds texture-matrix attributes to its own copies for these BMWR
resources, so using a live descriptor for an original draw would be invalid.

The original source is 165,088 bytes with raw FNV-1a `b3c38a9a41e5b793`.
Its native canonical identity is `c945defef88edfc9`: 2,301 positions, 2,282 normals,
8 shapes, 133 envelopes and 3,617 triangles. The independently decoded corner
hash is `fefc475b8f9a6309`. No game assets are distributed with this feature.

The standard Hero's Clothes source is 140,448 bytes with raw FNV-1a
`c00c6d9abd5d79bc`. Its canonical native identity is `0387df6437835108`: 1,560
positions, 1,733 normals, 18 shapes, 110 envelopes and 2,777 triangles. The native
corner hash is `7948281af98409cd`. Its original indices are preserved; the older
standard-outfit experiment that rebuilt per-corner indices remains retired.

Broader outfit support and artistic review remain separate work. macOS retains
best-effort support; native gameplay and Metal execution are unverified.

Windows D3D12 compares a frozen Ordon scene, exercises direct restoration,
reenable and native module reload, and closes cleanly. See the
[images and artifact-specific results](evidence/character-shading-2026-10-25/README.md).
The [standard-outfit repair record](evidence/standard-link-2026-10-26/README.md)
includes source-adapter regression checks, native outfit switching, re-enable,
module reload and exact restoration for Hero's Clothes.
