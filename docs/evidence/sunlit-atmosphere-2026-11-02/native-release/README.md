# Published sunlit-atmosphere package: native follow-up

The restored desktop permits focused Windows checks on 2026-11-02. No product
code or published binary changes are required. The [initial ledger](../README.md)
retains the disconnected-RDP failure and forced cleanup separately.

## Artifact and configuration

The published v1.21.0 Windows package SHA-256 is
`A770780DD61CF045E0D7474804BA640559DACB54FDB0CBAB19559A9C25E5ECFD`.
Its downloaded digest matches the GitHub release digest and the package copied
into this trial. The baseline host remains
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
`session.json` retains identities, initial settings and launch arguments;
`launch-published.ps1` is the launcher. No audit fixture is installed.

The trial uses D3D12 and Ordon F_SP103,0,27,0, an isolated empty card, autosave
off and external replacements disabled. Atmosphere alone runs at 35%; grading,
exposure, bloom, contact shading, shadows, bounce, shafts, local lights, enhanced
water, motion clarity and camera overrides remain off.

## Observations

The sunlit source activates naturally. Before reload, the log records view
direction (-0.369,0.721,-0.587), tint weight 0.387 and native RGB
(1.000,1.000,0.996). After reload it records (-0.369,0.699,-0.612), weight 0.500
and the same RGB. These rounded source values prove current native preparation
and reactivation; the log does not retain projected sun UV or sprite visibility.
The precise off-screen-continuity proof remains in the GPU and portable fixtures.

The independent atmosphere control switches off and back on, retaining 35%.
Live reload completes and the control remains On. World rendering continues
through fullscreen resize and return. Normal Alt+F4 close records exit zero in
`exit.json` and `all mods unloaded` in `native.log`.

Before reload, shared-composite shutdown reports compute=0, draw=4313 and
target_sets=0. Final close reports compute=0, draw=1870 and target_sets=0.
These are aggregate encoding counts, not GPU completion duration or per-frame
readback. Fog-only operation introduces no contact-field target set or compute
work. The sole logged error is the expected isolated empty-card
`Failed to open file: gczelda2`; no shader validation error is logged.

`control.jpg` and `reloaded-control.jpg` retain atmosphere On at 35%. `on.jpg`,
`off.jpg`, `resized.jpg` and `restored.jpg` retain visible native context.
Window captures are 962×752 including decoration around the configured 960×720
client; the fullscreen capture is 1800×1022. These are capture dimensions, not an
independent GPU attachment-size measurement. Animation, particles, native HUD
and toast state, cursor effects and JPEG encoding remain active. The images do
not establish isolated old-fog versus sunlit-haze quality or exact pixel parity.

`summary.json` retains source values, encoding counts, image dimensions and exit
status. `final-config.json` confirms atmosphere On at 35% after reload and close.
CI run 41505232158 passes Windows 30/30 tests and macOS 27/27 portable checks.
Both published packages pass contract and download-digest checks.

## Limits

The focused smoke closes the native activation and lifecycle gap. It does not
qualify a moving-camera gameplay route, whole-game art, weather/event transitions,
GPU duration, 4K cost or complete replacement-pack compatibility. macOS gameplay
and Metal remain unverified; support stays best effort through native builds
and portable checks. The failed initial native attempt is not reclassified as
a normal close by this later result.
