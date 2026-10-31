# Published bloom package: native follow-up, 2026-10-31

This closes the focused native smoke and lifecycle gaps recorded at the 1.19.0
release. No product code or published package changes are required. The
[previous ledger](../multiscale-bloom-2026-10-30/README.md) retains synthetic GPU
proof, the unavailable desktop capture and forced trial cleanup.

## Identity and configuration

The Windows package is downloaded from the published v1.19.0 release, SHA-256
`06D9E03CB313A830A000EB9F7E6EDE624337B5D6A73ABDFE8C9DA0EA83C0FD55`.
The exact-name baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
`session.json` retains both identities, initial settings and launch arguments;
`launch-published.ps1` is the launcher. No audit fixture is installed.

The trial uses D3D12, Ordon stage F_SP103,0,27,0, an isolated empty card, autosave
off and external replacements disabled. Bloom alone is enabled at 20%; grading,
AA, contact lighting, water enhancements, motion clarity and camera changes are
off. The initial window has a configured 960×720 client. `on.jpg`, `off.jpg`,
`control.jpg`, `reloaded-control.jpg` and `reloaded.jpg` are 962×752 captures
including window decoration. `resized.jpg` is a 1648×1098 fullscreen capture.
Capture dimensions are not an independent GPU attachment-size measurement.

## Observations

The released module discovers and draws the world. The independent bloom control
switches off and back on while retaining 20% strength. F11 changes to fullscreen
and returns to the initial window. Live reload completes without a stall; the
control remains On at 20% and visible world rendering continues afterward.
Alt+F4 closes normally, `all mods unloaded` is logged and `exit.json` records zero.

`native.log` reports 3,685 bloom compute callbacks, one retained target set and
five maximum levels at reload. Final shutdown reports 814 callbacks, one target
set and four maximum levels. These are aggregate callback counts, not GPU
completion duration, FPS or per-frame readback. The only logged error is the
expected isolated empty-card `Failed to open file: gczelda2`; no shader validation
error is logged. `summary.json` retains these assertions and capture dimensions.
`final-config.json` confirms bloom On at 20% after reload and shutdown.

The native captures use the same stationary camera but unfrozen animation,
particles, HUD and JPEG encoding. They document context and lifecycle, not exact
pixel parity, isolated artistic quality, broad camera motion or a gameplay route.
The earlier production GPU comparisons remain the precise halo/energy proof.

## Desktop recovery and remaining limits

Capture initially still fails with `foreground window did not report a process id`
despite RDP, helper and game sharing the active session. Fresh window selection
does not fix it. Resetting the JavaScript client and importing the supported
desktop API again recovers capture. The exact underlying helper failure is not
established. One stale screenshot identifier after resize also recovers with a
fresh observation. Neither recovery changes the product.

Whole-game art/motion, performance, 4K cost, replacement-pack coverage and macOS
gameplay/Metal remain unqualified. macOS support stays best effort; no hosted
Metal execution or additional product feature is introduced by this follow-up.
