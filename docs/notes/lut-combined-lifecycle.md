# Combined LUT lifecycle smoke check

## Windows D3D12, 2026-10-13

The isolated run `build/m13-aa-runtime/lut-combined-resize` uses package SHA-256
`ABEE0EB4F0DE421A6AC8D0FA6CF6D0D497860800B7DBE724B331F03C14046AA6`
with the host focus repair at `aec0cd4`. It starts Fishing Pond
(`F_SP127,0,0,0`) with grading, AA, enhanced water, and DOF enabled,
Natural at 100%, neutral exposure, and detail at 20%.

The scene renders before maximization, while maximized, and after restoring the
window. The player, pond, blurred background, and readable HUD remain visible.
The native Mods panel confirms the combined settings. Reloading MidnaFX retains
those settings and returns to rendered gameplay. Logs confirm deactivation and
reactivation, water absorption, and DOF compute/composite execution after reload.

Before reload, shutdown reports 1,967 look draws, one cached layout, and 1,876
water captures with zero failures. Final shutdown reports 1,797 look draws,
one cached layout, and 1,797 water captures with zero failures. All mods unload.
The first title-bar close click did not exit the host; a subsequent normal
Alt+F4 request did. Existing buffer-mapping/device-destruction teardown warnings
remain. The isolated run also reports the missing `gczelda2` save-card file.
No test host remains running.

Evidence JPEGs: `before.jpg`, `maximized.jpg`, `restored.jpg`,
`settings-before-reload.jpg`, `settings-after-reload.jpg`, and `after-reload.jpg`.
The clock was running, so lighting and actor motion differ between captures.
These images are lifecycle evidence, not matched visual comparisons.

This passes one combined Natural/AA/water/DOF maximize/restore/reload smoke case.
It does not establish driver-memory stability, every resize dimension, the full
combination matrix, save/load, GPU duration, or physical Mac behavior. A single
cached layout is not a GPU memory measurement. No product code, default, version,
or release asset changes are part of this check.

## Creative GPU control matrix

On 2026-10-13, the current Windows suite passed 16/16 tests in 28.95 seconds.
Review then identified that creative LUT control checks covered only `fs_main`;
the existing eight-entry-point matrix used identity tables.

`tests/aa_pixels.cpp` now exercises creative tables through all eight ordinary,
AA, detail, and water entry points in both RGBA8Unorm and BGRA8Unorm. Each of
the 16 combinations uses a 17-by-3 image with independent RGB values and varying
alpha. Six draws check baseline, exact zero-intensity bypass, Natural, Twilight,
half Twilight blend, and half intensity. Alpha must remain exact; each midpoint
must agree with independently quantized endpoint outputs within two code values
after doubling. Natural must differ from baseline and Twilight from Natural.

The expanded GPU test passed all 16 combinations and the existing checks in
35.07 seconds on local Windows D3D12, without a skip. Logs are
`build/m13-current-suite.log` and `build/m13-creative-matrix-test.log`.
This adds 96 GPU draws and changes tests only. The water inputs in this fixture
are neutral; this is a control-propagation check, not water optical correctness.
The new matrix has not yet run on hosted Metal. Physical gameplay, motion,
driver-memory measurements, and the remaining lifecycle matrix stay open.
