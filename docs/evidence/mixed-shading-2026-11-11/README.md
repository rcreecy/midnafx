# Broader rigid shading — November 11, 2026

Qualified rigid-model smoothing now defaults on for new settings and Remaster.
Explicit saved switches still apply. Mixed-material models protect all normals
used by transparent or otherwise unsupported materials, including shared uses
in opaque regions. Opaque-only qualification and restoration rules remain.

## Windows D3D12 observations

Original textures, clean default settings, updated local material-sheen host:

- Ordon `F_SP103,0,27,0`, native time 180: [capture](ordon.png).
  22 normal arrays applied and restored exactly, totaling 30,624 bytes.
- Forest Temple `D_MN05,3,0,0`: [capture](temple.png).
  15 normal arrays applied and restored exactly, totaling 15,648 bytes.

Both stages log a mixed-material candidate with 77 protected normal indices and
8 changed indices. Other mixed candidates correctly produce zero changes.
All previews close normally with exit code 0. No ownership-conflict, inexact
restoration or GPU validation errors are logged. The clean trials also log the
existing missing `gczelda2` card file.

These are loaded resource counts, not counts of visible enhanced objects.
The screenshots are unfrozen context, not isolated smoothing pixel comparisons.
Single-joint/matrix and deformation-history restrictions still limit coverage.
This adds no triangles, subdivisions, welded seams or smoother silhouettes.

## Focused checks and identity

Five CTest entries pass: rigid_shading, smoothing, qualification,
presets_roundtrip and package_contract. The qualification regression explicitly
checks fixed transparent normals, globally protected shared indices, retained
changes elsewhere and invalid protected indices. Existing restoration tests
cover intervening writers. The final package contract also passes after rebuild.

Ordon package SHA256:
`990F79D1F1F8ED9B684C8BB1D34F5D9F81209A28A8850A50AAC014718FD7EB4C`.
Final package / Temple trial SHA256:
`924BD3CED760720941B11FA977338E3E6690CBD71742D506250E4012DCFA6CD6`.
The final rebuild only adds protected-vector allocation to reported peak memory.
Host SHA256:
`FDBDD3F535351732538BC379A7E75BCCD6707D2F3C8BEF744C680EC3DD11BEA3`.

GPU cost, whole-game artistic consistency, mixed-material toggle/reload motion,
and macOS gameplay remain unverified. The existing restart requirement after
mod reload remains in force for automatic rigid qualification.
