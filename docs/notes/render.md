# Renderer feasibility research

Inspected 2026-09-16. Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`; its Aurora gitlink `7f2801cd0133c9333eadb4e2e6b24100c328d328`. Paths below are relative to `upstream/dusklight`, except Aurora paths prefixed `extern/aurora`. These are source findings, not runtime or performance validation.

## Conclusion

M1 grading is feasible through public native mod APIs: obtain the current scene color snapshot in `GFX_STAGE_FRAME_BEFORE_HUD`, then enqueue one fullscreen triangle using `GfxService`. No game-memory offsets or renderer patch are required. This API incurs a scene snapshot copy and a render-pass break; do not claim zero-copy or free grading. Bypass must return before resolving or pushing any work. Metal execution and 4K overhead remain to be measured on the target Intel Mac.

## Public API and exact call chain

`sdk/include/mods/svc/gfx.h` exposes service ID `DUSKLIGHT_SERVICE_ID_PREFIX "gfx"`, major 1, minor 2. Mods need `add_mod(... FEATURES webgpu)` (as in `mods/ao_mod/CMakeLists.txt`). All service calls run on the game thread. `docs/modding.md`, Graphics service section, describes this interface.

Relevant signatures (all are members of `GfxService`):

```cpp
ModResult (*get_device_info)(ModContext*, GfxDeviceInfo*);
ModResult (*get_scene_target_layout)(ModContext*, GfxRenderTargetLayout*);
ModResult (*register_stage_hook)(ModContext*, GfxStage,
    const GfxStageHookDesc*, GfxStageHookHandle*);
ModResult (*register_draw_type)(ModContext*, const GfxDrawTypeDesc*, GfxDrawTypeHandle*);
ModResult (*resolve_pass)(ModContext*, const GfxResolveDesc*, GfxResolvedTargets*);
ModResult (*push_uniform)(ModContext*, const void*, size_t, GfxRange*);
ModResult (*push_draw)(ModContext*, GfxDrawTypeHandle, const void*, size_t);
ModResult (*create_pass)(ModContext*, uint32_t width, uint32_t height);
```

Stage callback: `void (*GfxStageFn)(ModContext*, const GfxStageContext*, void*)`. Draw callback: `void (*GfxDrawFn)(ModContext*, const GfxDrawContext*, const void* payload, size_t payload_size, void*)`. Draw payload maximum is 128 bytes; use streamed uniforms for larger grading state. Draw callback executes later on the render worker; only raw `wgpu*` calls and provided context handles are allowed there, not other mod services.

The stage enum contains scene begin, after terrain, after opaque, frame before HUD, and frame after HUD. `src/m_Do/m_Do_graphic.cpp:2759`, in the painter, calls `gfx_run_stage(GFX_STAGE_FRAME_BEFORE_HUD)` after scene/wipe processing and before `dComIfGd_draw2DOpa`, `drawItem3D`, `draw2DOpaTop`, `draw2DXlu` and 2D particles. `FRAME_AFTER_HUD` occurs at line 2820. Pre-HUD has null `game_view` and `game_viewport`; world-camera stages carry those pointers. This is a precise render boundary, not a guarantee that every screen-space game effect is excluded: wipes precede it, some fades and item rendering follow it.

`src/dusk/mods/svc/gfx.cpp::gfx_run_stage` collects active stage callbacks and drains GX recording with `AuroraGXSync` around them. It rejects callbacks that leave a temporary offscreen pass open. Registration order is not a public priority contract; do not rely on ordering against arbitrary third-party callbacks at the same stage.

## Scene access and fullscreen rendering

Use `GFX_RESOLVE_DESC_INIT` (color true, depth false), `GFX_RESOLVED_TARGETS_INIT` and `resolve_pass`. Returned fields are borrowed single-sample color texture view, color format, width and height; depth can independently be requested but may be unavailable. Scene grading needs no depth. `src/dusk/mods/svc/gfx.cpp::gfx_resolve_pass` forwards to Aurora and refuses to resolve a game-owned offscreen pass.

`extern/aurora/lib/gfx/recording.cpp::resolve_pass` (line 961) acquires a snapshot, seals the current EFB pass, and resumes an EFB pass that loads existing contents. `acquire_pass_snapshot` (line 149) uses per-frame-slot pools and allocates/resizes textures only when needed. Color snapshot usage is `CopyDst | TextureBinding`, sample count one, surface color format. `extern/aurora/lib/gfx/encoding.cpp:299` records an actual `CopyTextureToTexture` of the complete scene color into this snapshot after resolving the scene pass. Thus the snapshot can be sampled safely while writing the continued scene target.

`mods/ao_mod/src/mod.cpp::on_draw` demonstrates a bind group referencing the snapshot and streamed uniform buffer, `wgpuRenderPassEncoderSetPipeline`, `SetBindGroup`, and `Draw(pass, 3, 1, 0, 0)`. Follow this fullscreen triangle pattern. Build pipelines from `get_scene_target_layout`, initialize every attachment with `gfx_init_color_target_states`, write scene attachment index zero only, retain depth format/sample count compatibility, disable depth writes and use Always depth compare. Host restores pipeline, bind group, viewport and scissor state after custom draw. Set a full-target viewport/scissor within the draw if needed.

`create_pass(width,height)` offers single-sample temporary targets; `resolve_pass` closes them and returns to EFB. No temporary mod-created pass is required for the initial one-pass grader or same-snapshot lightweight sharpening. Sampling the live render attachment directly is not an alternative exposed by this API.

## Formats, backend and color interpretation

`extern/aurora/lib/webgpu/gpu.cpp::best_surface_format` favors `RGBA8Unorm`/`BGRA8Unorm`; `to_linear` maps corresponding sRGB formats to non-sRGB variants. `extern/aurora/lib/gfx/frame.cpp::color_format` uses the surface configuration format, and `scene_render_target_layout` reports actual frame buffer size, format, depth and MSAA. The presentation WGSL in `gpu.cpp` samples and returns RGB without a gamma transform. These inspected paths support an SDR UNORM/display-value grading default. A non-sRGB storage format alone does not prove physically linear scene-light values; do not silently add sRGB decoding/encoding or advertise HDR precision. Query the format instead of hardcoding RGBA, and validate neutral appearance against captures.

The mod interface is WebGPU C API plus WGSL. Aurora uses Dawn native backends. `extern/aurora/lib/aurora.cpp::PreferredBackendOrder` includes Metal when compiled; `lib/webgpu/gpu.cpp` maps `BACKEND_METAL` to the WebGPU backend. `src/dusk/imgui/ImGuiConsole.cpp::try_parse_backend` exposes host backend choices. `GfxDeviceInfo` includes borrowed instance and adapter, so standard adapter metadata can inform diagnostics; rendering should stay backend-independent. No direct Metal code is necessary. Actual Intel Mac availability/support/performance is untested here.

## Resource ownership and synchronization

All service WGPU handles are borrowed. Callback context handles last only for that callback; resolved views last for the current frame. Do not save scene snapshots for future frames, release them, or create a persistent bind group referencing them. Record the view in copied POD draw payload and create/release its bind group during the draw. Stream uniforms using `push_uniform` so state remains immutable for the queued frame.

Own only durable pipelines, layouts, samplers and loaded shader source in M1. No mod-owned resolution-sized target is necessary. Handle layout changes before publishing a draw, and do not release or mutate objects that an earlier queued draw may still reference. A robust cache retains pipeline variants until shutdown and passes the exact variant in the payload rather than reading mutable global pipeline state from the worker.

`src/dusk/mods/loader/loader.cpp::deactivate_mod` invokes service deactivation before shutdown (line 297). `src/dusk/mods/svc/gfx.cpp::gfx_mod_deactivating` unregisters Aurora draw/task types and calls `aurora::gfx::synchronize`. `extern/aurora/lib/gfx/frame.cpp::synchronize` forwards to `render_worker::synchronize`, whose queue barrier at `lib/gfx/render_worker.cpp:254` waits for previous CPU encoding work. This is not a GPU-idle fence. Release owned WebGPU references in `mod_shutdown`, letting WebGPU retain submitted GPU resources; never call destructive texture operations on in-flight shared views. The host device outlives mods.

## Compilation, examples and profiling

`mods/ao_mod/src/mod.cpp::create_shader_module` uses `WGPUShaderSourceWGSL` and `wgpuDeviceCreateShaderModule`; pipeline creation uses `wgpuDeviceCreateRenderPipeline`. Source is loaded once from bundled mod resources. Compile at initialization or when a previously unseen layout appears; slider edits only alter uniforms. Runtime shader recompilation on tuning is unnecessary. Aurora's internal `lib/gfx/pipeline_cache.cpp` is not a public mod pipeline cache API: MidnaFX owns its pipeline cache.

Examples: `mods/ao_mod` provides depth resolve, compute and fullscreen composite; `mods/shadow_mod` provides multiple stage hooks and camera/depth work; `mods/window_demo` provides present targets. Avoid copying AO's frame-count-based retirement as a general synchronization guarantee.

No timestamp-query or profiler function is declared in public `GfxService`. Aurora internally marks snapshot work with `webgpu::gpu_prof::Zone` in `encoding.cpp`, but a mod should not assume access to host GPU timings. CPU stage-recording time excludes later worker encoding/GPU execution. GPU query support must be feature-checked before any future instrumentation, and target frame-time comparisons need controlled runtime captures.

## Remaining validation gates

- Exact Dawnlight ordering/version must be correlated with its source; this renderer finding alone cannot promise conflict-free bloom interaction.
- Neutral shader identity, coordinate orientation, 2D exclusion, MSAA variants, resize and unload need host runtime tests.
- SDR/gamma visual assumptions need representative Twilight captures; no invented quantitative quality claims.
- Measure enabled/disabled cost at the target resolution and backend. At 4K a full-resolution snapshot and read/write grading operation are bandwidth work even with few shader ALU instructions.
- Confirm missing/unavailable service behavior, shader compilation failure, unsupported layouts and missing snapshot all bypass safely with rate-limited diagnostics.
