# Contact-lighting reconstruction: 2026-10-29

## Scope and artifacts

This slice repairs a shared scene-lighting error rather than adding another
asset identity or tuning one prop. AO, contact shadows, color bounce and their
normal-guided filters use the reconstructed surface field. Their controls,
strengths, passes and target layouts remain unchanged.

The final local MidnaFX 1.16.0 Release package has SHA-256
`1E8541C7038390176096C0BFB7748AA6555A52FBA4C3AF7825AC29BE10EF4E0C`.
The published Windows 1.15 baseline package has SHA-256
`AFCDD3231D31B42DDAE047B2631C05C021116963C836D0112F4D605E45C19BF5`.
The exact-name native host has SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The [session](native-compare/session.json) and [reload receipt](native-compare/reload.json)
separate these artifacts. No game or replacement payload is included.

## Reproduced defect and rendering result

The old rule chooses the side with the smaller view-Z difference for each normal
derivative. A steep plane can therefore borrow its derivative from an unrelated
neighboring surface. The GPU fixture uses an analytic plane with normal
`(-6,0,1)` (or its transposed counterpart), clipped beside a separate wall at
distance 11. On a 96×96 target, a receiver at distance 10 has a same-plane
neighbor at approximately 11.538. The unrelated wall is closer in view Z, so the
old rule reverses the steep normal component. The selected wall lies behind the
plane-facing hemisphere and must not add AO at that receiver.

The new fixtures run unchanged against both production shaders. The baseline
shaders come from tag `v1.15.0`; the candidate is the final 1.16 shader source.
The [baseline GPU log](baseline-gpu.log) records twelve failed assertions:
eight analytic normals and four false-AO checks. All 44 preceding AO/shadow,
bounce, fog and sunlight assertions still pass. Four odd-target AO assertions
already pass under the old shader; no improvement is claimed for them.

The [candidate GPU log](candidate-gpu.log) records all 60 assertions passing on
Intel UHD Graphics 630, D3D12. Eight actual pipeline runs cover 96×96 and odd
39×27 targets, both image axes and forward/reversed depth. Each run compares the
decoded raw field normal with the analytic plane within two degrees. All eight
candidate normal dot products print as 1.000000; baseline values range from
-0.928500 to -0.660485. Square baseline AO visibility is 0.751953 at the receiver;
candidate visibility is 1.000000. Candidate rendered RGB stays within one source
code, with exact alpha. These assertions use analytic geometry and rendered
output rather than reproducing the derivative-selection rule as an oracle.

The existing 44 assertions retain raised contacts, directional shadows,
surface-aware filtering, depth-step isolation, bounce energy/headroom, fog,
sunlight, alpha, disabled paths and small/odd targets. The Release ALL build and
[three focused final checks](focused-final.log) also pass: shader compilation,
preset round trips and package contract. Product shader review finds no blocker.

## Native integration and limitation

The [launcher](launch-native.ps1) uses isolated configuration, D3D12 debug logs,
Ordon `F_SP103,0,27,0`, no fixture mod and no texture replacements. AO, contact
shadows and color bounce run together with motion clarity. Grading, detail, AA,
water, atmosphere, sunlight, exposure, bloom, geometry and camera changes are off.
Before/after configuration files retain the same enabled contact-lighting controls.

The initial [rate-zero screenshot](native-rate-zero.jpg) confirms frozen simulation
for the [baseline capture](native-before.jpg). Direct package overwrite fails
because the host retains its archive handle. Renaming the existing package to a
non-package extension permits installing the candidate at the original path.
The first reload request remains queued while simulation is frozen; resuming
with literal `rate 30` executes the reload. The [version panel](native-version.jpg)
then shows v1.16.0 active. The [candidate capture](native-after.jpg) and
[resized target](native-resized.jpg) show continued scene rendering.

The resumed simulation changes actor animation and native light selection.
These JPEG captures are native integration context, not matched pixel-quality
proof or an isolated effect comparison. The analytic GPU results establish the
specific corrected shading behavior. No whole-game native quality improvement
score, motion route, dungeon coverage or FPS gain is claimed.

The [native log](native-compare/native.log) reports all three effects active
before and after reload. Shutdown counters are 6,359 compute/draw pairs for the
baseline and 805 for the candidate, each retaining one target set. They count
encoded callbacks, not GPU duration or comparable frame times. F11 changes and
restores the viewport. Normal close exits zero and logs all mods unloaded. The
empty memory card reports `Failed to open file: gczelda2`; no shader validation
failure is reported. The [assertion script](analyze.py) produces the
[summary](summary.json) from these retained observations.

Reconstruction adds up to four depth reads and four view reconstructions per
valid half-resolution field pixel. No target or pass is added. GPU duration,
4K cost, every depth intersection and broad art/motion quality remain open.
macOS remains best effort through native package builds and portable checks;
gameplay and Metal execution are unverified on this Windows host.
