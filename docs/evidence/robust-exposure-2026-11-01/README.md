# Robust adaptive exposure evidence, 2026-11-01

Version 1.20 stabilizes the scene-wide meter with fixed stratified sampling and
weighted percentile bounds. The [feature contract](../../adaptive-exposure.md)
describes source luminance, fade gates, existing controls and unresolved cost.
No asset identities, stage exceptions, host patch or audit fixture are added.

## Focused GPU proof

`gpu-focused.log` records a successful D3D12 production-shader run on Intel UHD
Graphics 630. The original history group retains convergence, Rec.709 luminance,
brightening/darkening speeds, reset, fades, black bars and alpha assertions.
Eight new spatial groups exercise the replacement meter at different resolutions.
Run `build/windows-release/tests/Release/aa_pixels.exe --exposure-only` to repeat
these groups. Default `aa_pixels` execution still includes them alongside the
existing AA, grading, water, exposure-composite and LUT checks.

Bright and dark patches cover 1.56% of a 128×128 or 256×256 image and move through
eight one-pixel phases. Against uniform RGB 64, maximum metered-mean error changes
from 0.00714305 to 0.00024031 for white patches, and from 0.01373627 to 0.00024797
for RGB 4 patches. The reference is an independent CPU implementation of the
retired 16×16 point meter, not a copy of the replacement hash or histogram.

A 256×256 image has one-pixel white vertical stripes every sixteen pixels over
RGB 64. Sixteen one-pixel phases retain the same bright-pixel coverage while the
retired point grid can sample entirely white or entirely gray. Its metered-mean
range is 0.74901961; the replacement GPU meter's range is 0.00024414. The range of
the first 0.1-second exposure response from neutral changes from 0.12601293 to
0.00006866 stops. Every phase starts from neutral with the reset bit, so this is
spatial sampling proof rather than a long motion sequence or settled EV range.

Separate checks retain response to a half-bright image, deterministic repeated
sampling, alpha independence, uniform tiny/odd targets, fade gates and exposure
bounds. These fixtures do not prove all texture frequencies, global motion
quality or an exact continuous percentile. `focused-tests.log` retains the
passing shader compilation, preset persistence and 1.20.0 package contract.

## Native integration

The locally executed package SHA-256 is
`BA8E32FEB1FC306793310074B9902BE738BF0D078EC3230CAE6C6266365CA642`.
The exact-name baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
This local package is distinct from release-CI binaries. `session.json` retains
identity, launch arguments and initial settings; `launch-native.ps1` is the
launcher. D3D12 uses Ordon F_SP103,0,27,0, an isolated empty card, autosave off
and replacements disabled. Exposure alone is enabled; independent grading,
bloom, contact lighting, water enhancements, motion clarity and camera changes
remain off.

The native control switches off and back on. World rendering continues after a
live module reload, a fullscreen change and return to the window. Normal Alt+F4
close exits zero and logs `all mods unloaded`. `native.log` reports 2,296 exposure
callbacks before reload and 1,792 afterward, both with 4,096 samples and 10..90
percentile bounds. Counts establish encoded activity, not GPU completion duration
or per-frame readback. The only logged error is the expected empty-card
`Failed to open file: gczelda2`; no shader validation error is logged.

`on.jpg`, `off.jpg`, `control.jpg`, `resized.jpg` and `restored.jpg` retain native
context. Animation, particles, HUD/toast state and JPEG encoding remain active;
these are not isolated pixel-quality comparisons. Capture dimensions in
`summary.json` include decoration where present and are not independent GPU
attachment-size measurements. `final-config.json` retains exposure On after
reload and shutdown. `exit.json` records zero.

Whole-game art/motion, dungeon/combat/cutscene behavior, GPU duration, 4K cost
and complete replacement-pack compatibility remain open. macOS stays best effort
through native package builds and portable checks, without gameplay or hosted
Metal execution on this Windows host.
