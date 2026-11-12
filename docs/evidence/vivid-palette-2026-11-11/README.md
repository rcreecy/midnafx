# Richer Natural palette — November 11, 2026

The Natural lattice now emphasizes muted midtone color, green separation and
luminance contrast, with a reduced boost for warm red/orange regions and already
saturated colors. Neutral and the Twilight-specific lattice are unchanged.

## Native observations

Windows D3D12, original textures, default MidnaFX settings, native time 180:

- Ordon `F_SP103,0,27,0`: [capture](ordon.png).
- Faron Spring `F_SP108,1,3,0`: [capture](faron.png).

Both scenes render with the revised package and close normally with exit code 0.
No WGSL/GPU validation errors are logged. The isolated clean trials report the
existing missing `gczelda2` card file. These are unfrozen visual observations;
shader warmup, native clock, actor and foliage motion prevent exact comparison.
The prior Ordon palette is visible in the [material sheen capture](../material-sheen-2026-11-11/ordon-after.png).

## Validation

Four focused CTest entries pass in 155.54 seconds:

- `lut_palette`: identity lattice, finite bounded Natural lattice, alpha, black/
  white endpoints, monotonic neutral ramp, foliage/blue chroma and warm-tone bounds.
- `aa_pixels`: existing GPU matrix, including creative LUT endpoints, neutral
  ramps, intensity bypass, Twilight interpolation, alpha preservation and ordinary/
  AA/detail/water entry points in RGBA and BGRA formats.
- `presets_roundtrip` and `package_contract`.

Package SHA256:
`7EDC1A16D1EB45F67B038E9D5E412FA5DC00C096A6D2D5CCCCE966D3A73E4225`.
Host SHA256 (unchanged from material sheen):
`FDBDD3F535351732538BC379A7E75BCCD6707D2F3C8BEF744C680EC3DD11BEA3`.

Only CPU lattice generation changes. Runtime LUT size, bindings and per-pixel
work remain unchanged. These checks do not establish every scene's artistic
quality, true material/skin classification, HDR behavior or macOS gameplay.
