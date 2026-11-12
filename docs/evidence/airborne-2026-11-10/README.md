# Vegetation particles — November 10, 2026

Windows D3D12, original textures, default MidnaFX settings, Faron Spring
`F_SP108,1,3,0`. Native console time 180 (noon) and 0 (midnight).
The dialogue camera remains active; these are unfrozen visual observations.

- [Daylight pollen](day.png)
- [Nighttime fireflies](night.png)

Activation logs report `152 pollen, 0 fireflies` by day and
`0 pollen, 13 fireflies` at night. No WebGPU validation errors appear.
The isolated clean trial logs an unrelated missing `gczelda2` card file.
Normal Alt+F4 close returns exit code 0.

Package SHA256:
`87FDF948A254BB083E1068FDDD1A404D1BD4484B1E6FDA4ED34AF76A630114DC`.
Host SHA256:
`6AFEF114F9F012E397793D4A18A325E28EF2FB4BC8A92AC791E8AA04FAB601B7`.

Focused CTest: airborne_motion, native_material_compat, presets_roundtrip and
package_contract all pass. The motion test exercises the runtime math directly.

This does not establish whole-game coverage, GPU performance or macOS gameplay.
The existing terrain classifier determines eligible vegetation. Indoor,
underwater and Twilight views deliberately bypass the added particles.
