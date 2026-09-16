# M4.1 detail algorithm decision

The target is a native 3840×2160 scene on an older Intel Mac GPU. No target-GPU timing
or visual comparison is available yet. The selected first implementation is a bounded
five-tap cross high-pass filter in the existing pre-HUD pass. It is optional and defaults
off. A separate cached fragment entry point keeps ordinary grading at one
`textureLoad` per pixel; changing detail strength updates uniforms, not pipelines.

| Candidate | Source reads/pixel | Additional work | Decision |
| --- | ---: | --- | --- |
| Five-tap cross | 5 | Four-neighbor average, local-contrast limiter, bounded high-pass | Selected: least costly neighborhood with horizontal and vertical information. |
| Nine-tap 3×3 | 9 | Wider average/convolution, more bandwidth and diagonal contribution | Rejected pending evidence that diagonal quality is worth four more reads. |
| Lightweight unsharp mask | 5–9 in one pass, or extra full-size blur target | Depends on blur approximation; an actual Gaussian blur adds a pass/target | Same cheap cross neighborhood is effectively a small unsharp approximation. Extra target conflicts with current budget. |
| FidelityFX CAS / RCAS | Common implementations use a five-tap cross and adaptive weights | More arithmetic/permutations and implementation complexity; can reduce artifacts | Useful reference, but an exact port is unverified for the current WGSL/Intel Metal path. Test simple limited cross first. |

AMD's [CAS sample](https://gpuopen.com/manuals/fidelityfx_sdk/samples/contrast-adaptive-sharpening/)
documents sharpening before UI composition and implementation permutations; the
[RCAS source](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/ffx-fsr/ffx_fsr1.h)
documents its five-tap cross and clipping-aware weights. MidnaFX does **not** claim to
implement either AMD algorithm. The selected filter reads source RGB at center, north,
east, south, and west. Coordinates are explicitly clamped to the texture bounds before
`textureLoad`, so corner/edge pixels repeat the nearest valid texel. It computes

`highpass = center − (north + east + south + west) / 4`

and adds at most ±0.08 per channel multiplied by a prepared strength of 0–0.5. A local
contrast gate fades sharpening across strong edges, and a small high-pass gate avoids
amplifying near-flat grain. Output is clamped to 0–1 in the observed scene UNORM
domain. These measures reduce but cannot guarantee the absence of ringing, halos,
or texture-pack noise amplification; the runtime capture must check them.

Detail runs **before** fused grading, on the source scene RGB. Thus the five scene
samples feed one grading evaluation; sharpening after grading would apply the grading
curve to all five taps or operate on an extra intermediate image. Alpha is always taken
from the center source pixel. The pre-HUD stage excludes ordinary HUD composition,
though native 2D effects drawn before that stage can still enter the source snapshot.
All modes reuse that one snapshot and one fullscreen draw.

| Resolution | Pixels | 1 read/pixel | 5 reads/pixel | 9 reads/pixel | Nominal 4-byte source bytes at 5 reads/pixel |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1920×1080 | 2,073,600 | 2.07M | 10.37M | 18.66M | 39.55 MiB |
| 2560×1440 | 3,686,400 | 3.69M | 18.43M | 33.18M | 70.31 MiB |
| 3840×2160 | 8,294,400 | 8.29M | 41.47M | 74.65M | 158.20 MiB |

These are shader read counts and byte-count upper-style arithmetic, **not measured
physical memory traffic**. Neighboring fragments can share cache lines and tile data;
the scene copy, target writes, and host pass transitions remain additional costs. A/B
split can skip the four extra taps on its original left half. Detail-disabled Final mode
selects the one-read grading pipeline, so it performs no detail neighbor reads.

Debug views use the source snapshot and processed RGB. Luminance uses the same
`dot(rgb, vec3f(0.2126, 0.7152, 0.0722))` coefficients as saturation; these values
are in the host's currently observed UNORM scene domain, not physical luminance.
Highlight Clipping marks processed channels at ≥0.98 magenta and ≥0.90 amber. Shadow
Clipping marks processed luminance at ≤0.02 blue and ≤0.08 cyan. Other pixels are
dimmed processed RGB. Difference shows `clamp(4 × abs(processed − source), 0, 1)`.
These thresholds are display-bound diagnostics, not HDR clipping or byte-parity tests.
The A/B boundary is `floor(width × split_percent / 100)`; pixels left of it show the
source directly. Each view retains the center pixel alpha.
