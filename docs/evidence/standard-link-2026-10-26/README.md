# Standard Link support and re-enable repair

The final local v1.4.1 package supports Ordon and Hero's Clothes using the
fingerprinted original resource, descriptors and draw lists. It does not modify
native optimized draw copies. The source adapter is bounded and read-only;
triangle count, original corner hash and normal layout remain required.

Before the repair, the standard body's decoded position hash remained
`1a64a7d75b9baeae` and descriptor hash remained `786f6eaf069e363a`. Its native
heap draw hash changed from `e8443159db0e3e8a` to `22763bd9329ca1b3`, while the
original resource remained unchanged. Shape 15, group 1 then repeated a position
index. Re-enable decoded 2,776 triangles and one degenerate instead of the
original 2,777 triangles and rejected the model. The exact writer of that later
draw change is not established. See `native-before-fix.log`.

The original-source path also needs original descriptors: both Link resources
use BMWR, whose native warp-material setup augments heap draws and copied
descriptors with texture-matrix attributes. The source path pairs original SHP1
draws with the original converted descriptor records. It leaves native rendering
copies and their state intact.

The final Windows D3D12 run uses the qualified baseline host, stage
`F_SP103,0,27,0`, isolated copied save data, autosave off and external texture
replacements. Native editor outfit switching exercises the normal game change
path. The final package produces:

- Ordon: 297 changed normals, 1,402 preserved indices, two preserved uncertain
  faces, zero unresolved conflicts or faces. Outfit departure restores all
  13,692 normal bytes exactly.
- Hero's Clothes: 194 changed normals, 1,044 preserved indices, zero unresolved
  conflicts or faces, matching all 2,777 original triangles and corner hash
  `7948281af98409cd`.
- Standard-outfit disable restores all 10,398 normal bytes exactly, with original
  FNV-1a `2e9eeeef4f38da8e`. Re-enable and native module reload each apply the same
  194 changes from original draws after gameplay.
- Reload and normal shutdown restore those same bytes exactly. The owned host
  logs `all mods unloaded` and exits with code zero; stderr is empty.

See `native-final.log`. The final native run is a lifecycle/compatibility check,
not another matched art comparison. The
[earlier matched images](../standard-link-2026-10-25/README.md) are recorded
separately with their candidate package identity.

Final local package SHA-256:
`BC37D1AD39469225ACD870CCC33C0A3DDD08A32B4B37D2BC895CED22675ED1D4`.
Host SHA-256:
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
These are local artifacts, not the independently built CI package hashes.

Five affected CTest entries pass: character identity/source parsing, topology,
smoothing, profile persistence and package contract. Synthetic source-adapter
checks cover remapped shapes, original converted descriptors, truncation,
duplicate attributes/blocks, invalid modes, unterminated lists and oversized
offsets. No game assets are shipped. Mac gameplay and Metal remain unverified.
