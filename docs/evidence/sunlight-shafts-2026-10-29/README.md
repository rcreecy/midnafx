# Sunlight shafts: 2026-10-29

## Artifact and configuration

The final local MidnaFX 1.14.0 Release package has SHA-256
`32D26DA7F6498FCBB1B598A42EB5508F8F4CCDDDEA989B1A0AFD8361E8FCBDAB`.
The native Windows host has SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
[Session receipts](native-noon/session.json) separate product, native host and
temporary audit fixture identities. The fixture does not ship.

The [launch script](launch-native.ps1) uses an isolated user directory, D3D12 debug
logging and Ordon `F_SP103,0,27,0`. Auto-save, replacements and other scene effects
are initially disabled; sunlight strength is 10%. Motion clarity remains enabled.
The [fixture](driver.cpp), with its [build definition](driver-CMakeLists.txt), sets
native time to 180 once and uses the CameraService operator to aim a fixed eye
toward the real native sun. It never forces sun position, color, alpha or
visibility. It captures 300 unique safe frames, then stops CSV output while
retaining the camera until module unload. The initial two fixture revisions use
time 150; the first lacks the extra depth/weather diagnostic columns.

## Automated proof

The Release ALL build and [five focused tests](focused-tests.log) pass: sunlight
source preparation, profile round trips, shader compilation, ambient GPU pixels
and native package contract. The portable source test covers independent UV and
view transforms, perspective checks, beyond-far sun sprites, positive w,
off-screen rejection, alpha/visibility scaling, strength/color clamps, missing
dimensions/pointers and non-finite inputs. Earlier scene mask 511 leaves the new
bit off; the full scene-mask range now accepts 1023.

The [GPU suite](gpu-tests.log) passes all 44 cases, retaining existing AO, contact
shadow, bounce and fog checks. New cases establish:

- Default 10% sky contribution at 96×80 and 65×49, with 6,744 and 2,527 changed
  receivers. Independent linear-light headroom bounds and source alpha pass.
  Swapping warm/cool source colors changes the expected output hue.
- An opaque radial strip reduces the effect at an unchanged sky receiver depth.
  All-opaque visibility preserves exact source color. Equal radial visibility
  produces less veiling at 10 units than at 5,000 units.
- Zero strength retains existing AO/fog colors and fields exactly. Invalid or
  off-screen source data and invalid receiver depth bypass safely.
- The active AO/fog composition changes 2,300 distant receivers with zero
  headroom-bound violations. Matched reversed-Z geometry agrees within one output
  code and preserves alpha. Small 1×1 and odd 3×5 targets remain bounded.

The [initial test failure](initial-test-failure.log) is retained. Its synthetic
opaque horizon passes exactly through the sun, so every radial tap from a
foreground receiver remains opaque. Requiring positive foreground shafts there
is an impossible fixture condition. Moving the horizon below the sun makes sky
visible along the segment; the positive-light, alpha and energy assertions remain
unchanged. No production shader change is made for this failure.

## Native source qualification and integration

The [assertion script](analyze.py) produces the [summary](summary.json) from
[initial](native-on/), [diagnostic](native-probe/) and [noon](native-noon/) raw
observations. All three 300-frame programs complete and close normally with exit
zero. The initial two time-150 observations have zero native visibility and zero
prepared strength despite a centered sun and positive alpha.

The diagnostic observation rules out a weather/snow veto: both values are zero.
Native center depth is 16,777,148, below the clear-sky threshold 16,777,215;
three neighboring samples reach the threshold. The native sun visibility routine
requires the central sample for its full-visibility rise. This explains the zero
visibility without assuming a broken depth-readback path or changing product
gates. These CPU observations do not identify the exact occluding draw.

At native time 180, all five samples become clear. The noon program contains
295 positive source frames, ends with visibility 1 and prepared strength 0.1,
and keeps its sun UV at (0.5, 0.5). The product logs an active source during the
native visibility rise, then logs strength 0.100 after package reload. The
[daylight capture](native-daylight.jpg) shows this native context; it includes the
game's own sun sprite, flare and exposure response and does not isolate our effect.

Before enabling other Remaster effects, native reload records
`compute=0 draw=2120 target_sets=0`. This establishes that shafts-only operation
uses the existing composite draw without AO/bounce compute or field targets.
Selecting Remaster turns the [Basic control On](remaster-control.jpg). The toggle
changes independently to [Off](control-disabled.jpg) and back On; editing it marks
the profile Custom. The full profile continues rendering after fullscreen resize;
the [capture](native-resized.jpg) is 1868×1066. This image size is an observation,
not a GPU target-size readback. Contact shading, exposure, bloom and qualified
water captures continue afterward with zero water capture failures.

The [runtime log](native-noon/runtime.log) records all mods unloaded; the
[exit receipt](native-noon/exit.json) is zero. No shader or binding validation
errors appear. The isolated empty memory card reports `Failed to open file:
gczelda2`; that diagnostic is retained in [stderr](native-noon/stderr.log).

## Limits

GPU fixtures establish contribution, occlusion, composition and bounds. Native
logs establish source qualification and encoded draws, not per-pixel shaft-hit
counts. The screenshots are unsynchronized integration observations, not matched
quality comparisons; native flare, environment animation and downstream effects
remain present. Whole-game motion quality, foliage stability and GPU cost remain
unqualified. Off-screen and transparent/cloud occlusion cannot be reconstructed
from the opaque snapshot, though native visibility still modulates the effect.
macOS gameplay and Metal execution remain unverified and support is best effort.
