# Soft material highlights — November 11, 2026

The new host adds a broad native-light highlight to supported opaque lit draws.
The existing material relighting switch controls it. The new host is promoted to
`build/material-relighting/host-final/dusklight.exe`; the previous full host
remains in `build/material-relighting/host-before-sheen/`.

## Native observations

Windows D3D12, original textures, clean default mod settings:

- Ordon `F_SP103,0,27,0`, native console time 180.
  [Previous host](ordon-before.png), [new host](ordon-after.png).
- Forest Temple `D_MN05,3,0,0`: [new host](temple.png).

These are unfrozen observations. Animation, vegetation, particles, shader warmup
and the advancing clock prevent isolated pixel attribution. The native fallback
renders while enhanced shaders compile. Both enhanced trials log the new sheen
shader and an enhanced pipeline ready for draw encoding. Neither logs WGSL or
GPU validation errors. All three previews close normally with exit code 0.
The clean trials also log the existing missing `gczelda2` card file.

## Artifact identity and focused checks

New host SHA256:
`FDBDD3F535351732538BC379A7E75BCCD6707D2F3C8BEF744C680EC3DD11BEA3`.
Previous host SHA256:
`6AFEF114F9F012E397793D4A18A325E28EF2FB4BC8A92AC791E8AA04FAB601B7`.
Unchanged mod SHA256:
`87FDF948A254BB083E1068FDDD1A404D1BD4484B1E6FDA4ED34AF76A630114DC`.

The Aurora patch passes forward checking against its pinned Git index and reverse
checking against the applied working files. CTest native_material_compat,
presets_roundtrip and package_contract all pass. These tests establish existing
service/preset/package behavior; the native observations exercise the new shader.

## Limits

The highlight is an art-directed approximation using existing light and normal
inputs. It does not recover true roughness, metalness, reflections or offscreen
lighting. Alpha-tested and blended materials do not receive the new sheen.
Mesh silhouettes and hard normal boundaries remain unchanged. The native checks
do not establish whole-game artistic quality, GPU cost or macOS gameplay.
