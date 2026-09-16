# M1 correctness review (Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`)

## Findings

### P1 — transient graphics failure permanently disables the render path

`src/render/renderer.cpp:108-132` sets `state=3` for any layout mismatch or unsupported layout and `state=2` for any failed `resolve_pass` or `push_draw`. The callback's first condition, `state != 1`, prevents all subsequent attempts. Dusklight returns `MOD_UNAVAILABLE` from `gfx_resolve_pass` when a game-owned offscreen pass is active or Aurora cannot resolve (`src/dusk/mods/svc/gfx.cpp:668-683`), and `gfx_push_draw` returns `MOD_UNAVAILABLE` when Aurora cannot queue the draw (`src/dusk/mods/svc/gfx.cpp:602-616`). Aurora's layout key changes with sample count, color/depth format, and attachment count (`extern/aurora/lib/gfx/frame_packet.hpp:124-134`; `extern/aurora/tests/render_target_layout_test.cpp:27-50`). A user switching to MSAA and back, or a single unavailable frame, therefore leaves the effect disabled until mod reload. Treat `MOD_UNAVAILABLE` as a per-frame skip; support layout changes by rebuilding the pipeline when a supported layout returns, or at minimum allow recovery when the original layout returns. Keep permanent failure for invalid API or GPU object creation errors.

### P2 — initialization assumes a non-null GPU device and instance after `MOD_OK`

`src/render/renderer.cpp:169-177` pushes WGPU error scopes through `device.device`, and `pop_scope` later waits through `device.instance`. Dusklight's `gfx_get_device_info` returns `MOD_OK` even when Aurora's device is null, and only conditionally fills instance (`src/dusk/mods/svc/gfx.cpp:1049-1085`). Normal startup calls `mods_init` after `aurora_initialize`, so this is an edge case rather than a demonstrated normal-start crash (`src/m_Do/m_Do_main.cpp:923-925`). Still, check both handles before raw WGPU calls. If absent, report graphics unavailable and retry on update instead of invoking WGPU with null handles.

### P2 — wait loop can hang indefinitely on unrecoverable WGPU wait failure

`src/render/renderer.cpp:45-57` retries `wgpuInstanceWaitAny` without a limit after every status other than `Success` or `TimedOut`. A persistent `WGPUWaitStatus_Error` would block `mod_initialize` forever. Return `false` on `Error` or an unexpected status, while continuing to poll on `TimedOut`. The existing `WGPUCallbackMode_WaitAnyOnly` makes this ABI-safe: its callback fires only inside a `wgpuInstanceWaitAny` call that passes the same future. After an error return, no code must wait on the abandoned future. Instance destruction marks the event cancelled but does not independently invoke a WaitAnyOnly callback. See the [WebGPU C asynchronous operation contract](https://webgpu-native.github.io/webgpu-headers/Asynchronous-Operations.html), especially callback modes and wait statuses. This does not require changing callback mode or extending stack storage lifetime.

## Checked without actionable finding

The snapshot view is frame-scoped and only passed through `push_draw` for the same frame, matching the `GfxService` contract (`sdk/include/mods/svc/gfx.h:15-31`). Stage callbacks run on the game thread; draw callbacks run on the render worker, and `draw` uses only its context and raw WGPU calls. `gfx_init_color_target_states` correctly masks non-scene attachments (`gfx.h:79-104`). The WGSL `textureLoad` passthrough is valid for the supported `RGBA8Unorm` and `BGRA8Unorm` formats; its full-screen triangle needs no vertex buffer. Width and height are not part of Aurora's layout key, so a resize can use the existing pipeline while the current dimensions are passed to `draw`. Mac build configuration targets x86_64 (`CMakePresets.json`), but an actual macOS build and Metal run remain unverified.

## Resolution

`src/render/renderer.cpp` now retries `MOD_UNAVAILABLE` on the next active frame, treats
unsupported layouts as recoverable, and retries an unsupported startup layout when grading
is enabled. Device and instance handles are checked before any raw WebGPU call. The
`WaitAnyOnly` error-scope wait returns failure on `WGPUWaitStatus_Error`, abandoning that
specific future; the cited callback contract prevents a later callback into unloaded mod
code. The Windows x64 build and package test pass after these fixes. Host runtime behavior
remains unvalidated.
