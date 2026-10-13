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
