# Edge-directed AA evidence, 2026-10-26

Version 1.8 upgrades the shared scene AA filter, not an asset allowlist. See
[the feature contract](../../edge-antialiasing.md) for controls and cost limits.

## Automated evidence

`gpu-final.log` retains the final Windows D3D12 shader, pixel and package checks.
All three focused CTest entries pass. The expanded pixel fixture also runs the
existing LUT, water, exposure and bloom composition cases. It executes production
WGSL, not a CPU copy of the new algorithm.

Six synthetic 65×65 half-planes compare binary center samples with independent
64×64-per-pixel supersampled area coverage. The reference geometry is fixed before
rendering. A retained CPU implementation of the retired five-tap kernel provides
the previous quality baseline. Mean squared error is scored over the interior
41×41 region; the 12-pixel border is excluded because endpoints outside the image
cannot be observed. Scores use encoded channel values, not linear-light energy.

| Half-plane slope | Raw source MSE | Retired filter MSE | New filter MSE |
|---|---:|---:|---:|
| +1 and −1, each | 396.493902 | 214.191701 | 208.976948 |
| +¼ and −¼, each normal/transposed | 155.484842 | 102.380291 | 30.536076 |

The shallow fixtures reduce error by about 70.2% against the retired filter;
the 45° fixtures improve by about 2.4%. These are fixed phases/orientations, not
a guarantee for every edge or a measurement of gameplay motion. Binary color
input cannot uniquely recover the original geometric coverage.

Additional independent invariants pass:

- Exact identity for high-contrast horizontal/vertical aligned steps.
- Exact full-frame transpose of a 33×33 shallow step, including borders and alpha.
- Brightness-complement symmetry within one output code value.
- Exact identity for horizontal, vertical and both diagonal one-pixel ridges,
  complemented diagonal valleys, checkerboards and an isolated pixel.
- Saturated-edge channel bounds, common RGB blend fraction and center alpha.
- Exact ordinary/inactive-water output parity, plus existing active-water,
  detail, LUT, exposure and bloom composition checks.

Review found a tie-direction bias, then diagonal-line widening in the first tie
repair. GPU checks exposed isolated-pixel and clamped-corner changes as well.
The final shader handles ties symmetrically, requires diagonal support and keeps
the outermost edge texels. `development-failures.log` retains the failed assertions
from intermediate builds; the final fixture passes without relaxing these
preservation assertions. The initially added supersampled coverage gates also
remain unchanged.

## Native Windows observations

The final local 1.8.0 package SHA-256 is
`ACB4FFF6E8EFB7E0C5BD1190DE87442E92B6537027858B597478A1F1B47A630C`.
The D3D12 baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The isolated session uses stage `F_SP103,0,27,0`, a 960×720 scene target, autosave
off and an empty memory card. Two DDS samples from
`build/asset-hot-reload-pack/A-first` are registered. This is not complete 4K-pack
verification, and no game/texture payload is included here.

`on.jpg` and `off.jpg` retain the same camera and simulation frozen with `rate 0`.
Only AA changes; grading, detail, exposure, bloom, AO, atmosphere, smoothing and
camera modifiers are off. Enhanced water remains enabled. Character/scene edge
smoothing is visible with the native HUD still present. Native water/cursor/HUD
animation can continue, and captures are JPEG, so the pair is not exact pixel
identity proof. It compares the new filter enabled/disabled, not the old and new
filters. `controls.jpg` records the independent switch and help text.

Literal `rate 30` resumes simulation. Selecting **Profile / Remaster** re-enables
AA with the other effects, then the native UI reloads the module. `reloaded.jpg`
and `native-final.log` retain the successful reload and continued scene rendering.
The saved profile remains Remaster with AA enabled. Normal close exits zero,
restores owned geometry and logs `all mods unloaded`. This slice adds no render
resources; another native resize matrix was not required or performed.

The empty card reports `Failed to open file: gczelda2`. After mod unload, the host
reports aborted buffer mappings and `Device lost: Device was destroyed.` These
teardown warnings are retained; no new shader validation error is logged.
Whole-game motion/art quality, GPU duration and 4K performance remain open.
macOS gameplay/Metal execution are unverified; native builds and portable release
checks continue without hosted Metal runtime tests.
