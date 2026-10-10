# M13 AA correctness and optimization review

Reviewed and repaired on 2026-10-08. This review covers shader math, grading
state, pipeline selection, snapshot/layout guards, resource ownership, and
default behavior. Desktop access was unavailable; GPU pixel tests use offscreen
D3D12 on Intel UHD Graphics 630.

## Repaired findings

1. **Disabled grading could still alter the frame.** AA and enhanced water can
   independently request a draw, but `prepared_grade()` returned saved grading,
   detail, and Twilight adjustments even when Enable grading was off. It now
   returns cached neutral uniforms with zero detail before Twilight blending.
   Diagnostic mode and difference gain remain available. Saved settings remain
   intact for re-enabling grading. This finding is established by the settings
   and renderer control flow; no live settings-panel verification is claimed.

2. **Detail could counteract AA.** Both shader families sharpened an AA-filtered
   center against unfiltered neighbors. This mixed two signals and could undo
   filtering at moderate-contrast edges. Detail now operates only where the AA
   detector rejects the neighborhood. The regression uses an 80/120 step edge
   at maximum detail strength: four nontrivial image sizes failed before repair
   and pass afterward. A separate low-contrast pattern checks that detail still
   works outside AA-filtered edges.

3. **The combined path repeated source sampling.** AA reloaded the center, and
   detail loaded all four neighbors again. The fragment entry now passes its
   center RGB to AA, and a detail helper consumes AA's existing neighbors.
   AA-only and AA/detail each express five scene texture loads, rather than six
   and ten respectively. Water optical sampling is additional and unchanged.
   This removes redundant source operations; compiler common-subexpression
   elimination may already have removed some loads, so no measured speedup or
   GPU timing claim is made.

## Default-on decision

The user explicitly requested default-on after repair. `anti_aliasing` registers
with default true; normal config loading preserves an existing saved false.
The control moves from Developer to Basic, retains an experimental label, and
explains its independence from grading and saved looks. Grading status no longer
claims the entire effect is disabled when AA is requested. Neutral grading alone
does not imply a render bypass while AA is enabled. No installed user config is
overwritten and no release is published by this change.

## Retained design and limits

The filter is a bounded five-tap FXAA-style approximation, not full reference
FXAA or a temporal method. It can soften thin features and straight edges;
default-on is a product decision, not proof of scene quality. Alpha is copied
from the center. Coordinates clamp at image edges. Diagnostic/passthrough paths
exclude AA. Unsupported layouts and snapshot mismatches retain existing guards.
Pipeline resources retain the existing layout-keyed ownership and shutdown path.
No new render pass or full-size target is added. Enhanced-water optics still run
after AA and can introduce edges that this pass does not filter.

The Windows build and all 16 CTest tests pass, including shader compilation for
ordinary and water variants, package validation, and 25 offscreen pixel cases.
The pixel test covers exact neutral identity, alpha, flat/low-contrast retention,
edge blending bounds, one-pixel dimensions, padded rows, zero-strength detail,
edge/detail separation, and low-contrast detail activity. Water variants receive
pipeline compilation coverage, not offscreen optical-composition comparison.
Live art quality, temporal shimmering, water boundaries, Metal, and performance
remain unqualified. Earlier default-off statements in dated investigation
entries describe prior checkpoints and are superseded by this decision.

## Release CI follow-up

v1.2.0 Windows CI passed and its package was published. The Intel Mac package
compiled, but aa_pixels failed on the hosted Apple Paravirtual Metal device,
including basic pixel identity. The cause remains unresolved. The Mac asset
was withheld; the release is Windows-only. This failure does not establish
behavior on physical Intel Mac hardware.

### Diagnostic rerun (2026-10-09)

Commit `7ba4f1b` adds bounded first-pixel mismatch reporting to `aa_pixels`.
It changes neither rendering code nor assertions and does not relax the gate.
The Windows pixel test passes locally after rebuilding.

Manual CI run [38012005349](https://github.com/rcreecy/midnafx/actions/runs/38012005349)
passes all 16 tests on both Windows and Intel Mac, including `aa_pixels`
(Mac: 3.70 seconds; it was not skipped). Both platform artifacts were produced;
the release job was skipped because this was a branch dispatch, not a tag.
Successful CTest output does not identify the adapter for this run.

This is a successful Mac CI observation, not a demonstrated fix for the earlier
Apple Paravirtual failure. No production shader or renderer change separates
the runs. The earlier failure is therefore not consistently reproduced across
these runner executions, and its cause remains unresolved. The added failure
diagnostics will distinguish missing readback data and pixel/channel mismatch
if it recurs. The published v1.2.0 release remains Windows-only; the new Mac
artifact has not been promoted to a release or qualified on physical hardware.

### Consecutive Metal verification (2026-10-09)

CI run [38017063663](https://github.com/rcreecy/midnafx/actions/runs/38017063663)
passes all 16 tests on both platforms. A new Mac-only gate then executes
`aa_pixels` with `--repeat until-fail:3 --verbose`: all three executions pass
all 25 cases, each identifying `Apple Paravirtual device; backend=5; type=1`.
The gate stops on the first failure rather than retrying until success.

This confirms repeatable synthetic pixel behavior on this hosted Metal runner
and a second successful full CI run. It does not explain the original failure,
prove reliability across every runner instance, or qualify live Mac gameplay.
No production rendering code changed. Diagnostics and the consecutive-run gate
remain enabled to preserve evidence if the failure recurs. Release assets are
unchanged; the Mac package remains a CI artifact.

### Intel Mac publication (2026-10-09)

The user explicitly requested Mac shipment without physical hardware available.
The Intel Mac artifact from run 38017063663 is now attached to v1.2.0 as
`midnafx-macos-x86_64.dusk`. Its manifest declares version 1.2.0 and includes
`lib/macos-x86_64/mod.so`. Comparing the build commit with the release tag shows
no changes to product rendering source. GitHub's uploaded asset digest matches
local SHA-256 `767C55A028A98A78A2670F6B56B27C327E1D51A0EDDC827240C32E26C39BCF66`.
Release notes disclose the earlier unexplained failure, passing reruns, lack of
physical Mac gameplay testing, and Intel-only architecture. AA retains its
experimental label. The existing Windows asset is unchanged.

