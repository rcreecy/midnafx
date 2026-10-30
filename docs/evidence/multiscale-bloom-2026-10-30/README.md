# Multi-scale bloom evidence, 2026-10-30

Version 1.19 broadens the existing scene-wide bloom while retaining its controls,
default strength and bounded highlight composition. The [feature contract](../../scene-bloom.md)
describes the rendering path, memory and unavailable performance evidence.

## Production GPU evidence

`gpu-final.log` retains fourteen passing D3D12 cases on Intel UHD Graphics 630.
The original eight checks remain; six added cases cover uniform-field bounds,
visible multi-scale spread, extraction-phase motion, odd borders, a five-level
one-column target and strength scaling. All five focused CTest entries pass in
`focused-tests.log`: portable planning, preset round trips, production shader
compilation, bloom pixels and the 1.19.0 package contract.

`source.png`, `core.png` and `multiscale.png` are exact RGB exports from the GPU
fixture's scene/composite readbacks. The synthetic 960×720 source has a 128×128
white patch on RGB code 12. Core-only and multi-scale outputs share extraction,
the original Gaussian filters, 20% strength and final composition. The reference
disables only the added pyramid; it is not a separate executable or native game
capture. Alpha is checked separately for every output pixel.

At least 24 pixels beyond the patch, the new halo changes 1,184 additional output
pixels. Integrated floating field energy changes from 71.61295509 to 71.38690197
(-0.316%). The retained core remains above 60% of the core-only center response.
`profile.csv` contains the center-row floating field samples; `summary.json`
retains comparison and movement measurements. These are LDR values, not physical
light units or isolated game material measurements.

Four one-pixel shifts of a 4×4 red highlight on a 512×384 black source retain
integrated energy within 1% and centroid error below 0.03 pixels. This checks the
field's response through extraction phases, not all final LDR pixels, temporal
reprojection, camera motion or game animation. The no-history check uses fresh
fixture resources; it does not independently exercise native texture reuse.

`initial-tests-failed.log` retains the first failed acceptance attempt. A smaller
512×384, 64×64-patch case added floating halo energy beyond 24 pixels, but no
additional quantized output pixels there. The final proof uses a native-sized
960×720 target and larger patch. Small/faint halos can still disappear under LDR
quantization; the implementation was not strengthened to conceal that limit.

## Native and platform limits

`launch-native.ps1` starts the exact-name, hash-qualified baseline host with an
isolated empty card, replacements disabled, bloom alone at 20%, and Ordon stage
F_SP103,0,27,0. No audit fixture is installed. Native window capture and activation
both fail with `foreground window did not report a process id`, including one
fresh-window recovery. No further UI action is attempted without a restored
desktop. Native visual, toggle, resize, reload and normal-close qualification are
therefore open; earlier 1.7 lifecycle observations do not qualify this version.
`session.json` retains host/package hashes and initial settings; `native.log`
records the 1.19.0 module discovery and startup. The owned trial process is
terminated for cleanup after desktop recovery remains unavailable. `cleanup.json`
records the forced termination; it is not a normal-close or exit-zero result.

macOS support remains best effort through native builds and portable checks.
Gameplay and Metal execution are unverified. No remote Metal execution is added.
GPU duration, 4K performance and whole-game art/motion quality remain open.
