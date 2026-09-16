# M2 correctness review (Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`)

## Finding

### P2 — a changed but still supported scene layout leaves grading bypassed

`src/render/renderer.cpp:151-154` stores state 3 whenever the current scene layout key differs from the key used to build the pipelines. Subsequent stage callbacks can leave state 3 only when the original key returns (`:155-157`); `update()` retries initialization only in states 4 and 5 (`:311-317`). A new supported layout therefore leaves grading bypassed indefinitely, even though `supported(current)` accepts it. Aurora's key includes color attachment count and formats, depth/stencil format, and sample count (`extern/aurora/lib/gfx/frame_packet.hpp:124-134`). Dusklight supplies the current layout on every query (`src/dusk/mods/svc/gfx.cpp:1088-1100`). Rebuild both pipelines against a changed supported layout, on a safe lifecycle path, and resume only after they and the draw/stage registration are valid. A resize alone does not trigger this because width and height are absent from the key. An MSAA transition back to the original key already recovers.

## Checks without an actionable finding

The eight `float` fields in `grade::Uniforms` match the WGSL uniform struct's 32-byte scalar layout. Aurora aligns each streamed uniform offset to `minUniformBufferOffsetAlignment` and returns the original byte count (`extern/aurora/lib/gfx/recording.cpp:245-257,1117-1123`), so the 32-byte bind-group entry in `draw()` is valid. The per-frame resolved view is used only in its queued draw. The controls clamp values before computing reciprocal gamma and black-point division. Fully neutral controls return before `push_uniform` and `resolve_pass`; the explicit passthrough comparison is the intentional exception. Config callbacks recompute a complete prepared value, and panel callbacks persist settings through `ConfigService`.

The review is based on source and the existing Windows build/tests. It does not establish WGSL execution on Metal or color quality in Dusklight.
