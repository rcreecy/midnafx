# Edge-directed scene anti-aliasing

Version 1.8 replaces the former five-tap edge blur with a shared edge-directed
spatial filter. The existing **Anti-aliasing (experimental)** switch controls it;
new configurations and Remaster keep AA enabled, while Vanilla disables it.
Existing saved settings are preserved. No texture pack, material registration or
stage configuration is required.

The previous filter mixed opposite neighbors at every detected contrast edge,
including perfectly aligned horizontal/vertical boundaries. The replacement
estimates orientation from a 3×3 neighborhood and searches along the boundary
for nearby endpoints. A bounded coverage estimate smooths stair steps, while
unbroken aligned edges retain their source values. Equal orientation scores use
symmetric normal contributions instead of choosing an arbitrary axis. Thin-ridge
rejection avoids spreading the tested axis/diagonal one-pixel lines and isolated
texels. The outermost high-contrast texels retain their source values when an edge neighborhood
is incomplete. These are spatial heuristics, not exact geometric reconstruction.

All blends remain within source/neighbor channel bounds, with a maximum 40%
contribution from neighboring colors. Alpha stays at the original center value.
Flat/low-contrast regions retain the existing optional detail treatment; detected
edges are not sharpened against unfiltered neighbors. Grading, bloom, exposure
and LUT ordering remain unchanged. The water shader uses the same filter before
enhanced-water treatment, preserving the existing integration; final water
highlights are not separately anti-aliased. The native HUD remains downstream.

The filter reuses the existing snapshot, draw and bindings. It adds no texture,
compute pass, temporal history, jitter, motion vector or host requirement. Flat
regions still load four neighbors. High-contrast regions may load eight local
neighbors and up to 28 endpoint samples, compared with four neighbors previously.
GPU duration and 4K performance remain unmeasured. It cannot remove all texture
shimmer, recover missing geometry, or provide TAA/SMAA/MSAA behavior.

The [evidence ledger](evidence/edge-aa-2026-10-26/README.md) records supersampled
coverage comparisons, preservation and symmetry regressions, production shader
composition and native Windows observations. Whole-game motion/art quality
remains open. macOS support stays best effort through native package builds and
portable checks; gameplay and Metal execution are unverified.
