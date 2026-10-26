# Filtered scene-wide contact shading

Version 1.6 replaces the former 16-tap full-resolution AO kernel with 32 samples
per half-resolution pixel, two surface-aware filter dispatches and edge-aware
full-resolution reconstruction. The path applies to opaque scene depth without
model identities, material fingerprints or stage lists. Existing AO settings and
Remaster use it automatically. See [the feature contract](../../ambient-occlusion.md).

## Focused GPU checks

Nine D3D12 pixel cases pass on the working host's Intel UHD 630. They cover flat
planes, odd/one-pixel dimensions, background identity, raised contacts, seeded
spatial variation, separated foreground/background surfaces, native fog and alpha.
An added assertion also passes exact fog parity with positive AO strength and an
all-invalid neutral field, representing unavailable AO.

The 96×80 raised-box scene darkens 940 pixels with a maximum red-channel drop of
26 code values. The odd 65×49 case darkens 512 pixels with a maximum drop of 16.
A seeded checker field loses more than 75% of its neighbor variation after
filtering while retaining its mean. A separate distant plane remains exactly
unchanged across the filter and reconstruction boundary. These are independent
synthetic invariants, not whole-game motion or art-quality results.

All production shader entry points compile. The final package contract passes
with version 1.6.0. Hosted Mac execution excludes both GPU pixel executables;
macOS package and portable checks continue under the existing support policy.

## Native observations

The comparison uses Windows D3D12 and the qualified baseline host, isolated user
data, autosave off, stage `F_SP103,0,27,0` and a 960×720 scene target. Two external
high-resolution DDS samples remain registered from the existing
`build/asset-hot-reload-pack/A-first` fixture. This is not the complete 4K pack.
No texture or game payload is included here.

`on.jpg` and `off.jpg` share the same frozen simulation and camera; only AO
changes. Grading, detail, camera modifiers, adaptive exposure, depth atmosphere
and model smoothing are off. AA and enhanced water remain on. Native HUD and
render-time water/cursor animation can still vary, so these JPEG captures are
not lossless pixel-parity proof. They demonstrate visible shading across Link,
terrain and rock contacts, rather than a single supported asset.

| Fixed rectangle (x,y,width,height) | Mean RGB off | Mean RGB on |
|---|---:|---:|
| Link (420,280,110,150) | 85.97 | 73.96 |
| Rock/ground (650,460,170,80) | 63.90 | 60.90 |
| Distant rock (530,120,120,65) | 86.49 | 85.60 |
| Open path (610,245,140,60) | 69.31 | 69.03 |

The mostly open path changes little while contacts receive more shading. These
JPEG code-value averages are not linear-light measurements. The pair compares
the new AO effect on/off, not the old and new algorithms against each other.

The comparison candidate package SHA-256 is
`A083067D218E314D879F463C9F7E158A5F2CDB817BE1FB40EE8783D127BF358D`.
It predates the rare AO-failure/fog fallback repair. `native-comparison.log`
records 295,451 compute/composite callback pairs, one retained target set and
`all mods unloaded`; normal process exit is zero. Callback counts do not measure
frame rate or GPU completion time.

The final local 1.6.0 package SHA-256 is
`E44177310C812D44E3FA48BB07D06903BA3618A1125737D4C48D4A8BC7776F3B`.
It includes the fallback repair and metadata change. The final native session
runs the combined Remaster profile, resizes from 960×720 to 1024×697, reloads the
module during normal 30 Hz simulation, then restores the original window size.
`resized.jpg`, `reloaded.jpg` and `restored.jpg` are lifecycle observations, not
matched art comparisons. AO, depth atmosphere, water and adaptive exposure
continue rendering. `native-final.log` records 2,434 compute/draw pairs before
reload and 1,077 afterward, with one target set retained at each shutdown. The
host logs `all mods unloaded` and exits with code zero.

Both isolated sessions report `Failed to open file: gczelda2` for their empty
memory card. No WebGPU error is reported in the final session. Host SHA-256:
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
Local artifact hashes differ from independently built CI packages.

## Review and limits

Source review found that a failed AO submission could suppress otherwise valid
atmosphere. The repair continues with a neutral field; queued composites also
select neutral data if compute fails. The focused fog assertion proves that
neutral-data composition retains haze even with recorded positive AO strength.
Failure injection into the native host was not performed.

Surface reconstruction uses the original full-precision depth texture, avoiding
half-float distance loss. Resize retirement counts queued callbacks rather than
assuming an in-flight frame limit. Resource creation checks consumed WebGPU
error scopes; adaptive exposure reuses the same checked-creation helper.

Whole-game motion/cutscene quality, thin contacts and 4K GPU duration remain open.
The two half-resolution fields add about 8 MiB at 1080p or 32 MiB at 4K, plus
temporarily retained resources during resizing. No speedup, temporal-stability
guarantee, off-screen shadowing or dynamic-light implementation is claimed.
Intel macOS gameplay/Metal execution remains unavailable and best effort.
