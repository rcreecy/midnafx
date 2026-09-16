# Integration review: M1 passthrough

Source review, 2026-09-16. Dusklight revision `edf42c6a7202647b56dd2fcdef02d17671bc814b`. No GPU runtime validation performed. Production implementation review remains pending.

## Feasibility and constraints

The supported graphics API provides a plausible passthrough route: record at `GFX_STAGE_FRAME_BEFORE_HUD`, resolve color into a distinct snapshot, then record a full-screen draw into the loading EFB continuation. `sdk/include/mods/svc/gfx.h` documents game-thread stage callbacks, render-worker draw callbacks, frame-only snapshot views, and host restoration of pipeline/bind-group/viewport/scissor state. Use GfxService 1.2 target layout rather than deprecated single-format fields.

`extern/aurora/lib/gfx/recording.cpp:963` (`resolve_pass`) drains GX FIFO, assigns snapshot destination, seals/enqueues current pass and resumes a loading EFB pass. `acquire_pass_snapshot` at 149 creates a single-sample CopyDst/TextureBinding image at target size and surface format. `extern/aurora/lib/gfx/encoding.cpp:300` copies from the sealed pass's copy source to this snapshot. Source and destination of the custom draw are thus distinct resources; sampling the live color attachment instead would be invalid.

For M1, restrict to **sample_count == 1** and supported RGBA8/BGRA8 unorm formats, use the snapshot view's actual format and size, load with `textureLoad(input, vec2<i32>(position.xy), 0)`, write all four channels unchanged, disable blending, use a full target viewport/scissor and a fullscreen triangle. Pixel-center integer truncation maps `(x+0.5,y+0.5)` to `(x,y)` when both images share the full target dimensions/origin. Do not add UV flips, gamma operations, dithering, filtering, color-only masks or alpha replacement. Exact byte parity remains a runtime test, not a proof from shader text.

MSAA is a material limitation: `recording.cpp:89/98` chooses the resolved framebuffer for snapshots. Writing that single resolved color back over a multisample attachment overwrites its original per-sample colors. The final resolve may initially look identical, but later partially covered/blended HUD geometry can produce different results. Gate M1 before resolving or drawing when multisampled; silently labeling this MSAA route exact passthrough would be incorrect.

## MRT and depth

Use `gfx_init_color_target_states` (`sdk/include/mods/svc/gfx.h:72`) with no blend and scene write mask All. It supplies every target format/count while setting non-scene write masks to None. WGSL needs only location 0 in this configuration; do not fabricate outputs to normal/auxiliary targets. Aurora's own `extern/aurora/lib/gfx/clear.cpp:21–145` demonstrates this pattern. Its pipeline also matches depth format/sample count. For passthrough, depthCompare Always and depth writes disabled; keep stencil writes disabled. Cache or reject changes using layout key plus relevant descriptors, never reuse a single-format pipeline against a changed MRT layout. The current EFB implementation asserts that extra scene attachments lack backing (`recording.cpp:94`), so compatibility with future MRT is a descriptor design requirement, not runtime coverage already achieved.

WebGPU permits absent fragment outputs when the corresponding target write mask is zero; target formats and sample counts must still agree with the render pass. See the [WebGPU specification](https://www.w3.org/TR/webgpu/). Source-equivalent output does not bypass format conversion, and sRGB formats introduce decode/encode behavior; M1 should explicitly reject formats outside its tested contract.

## GPU errors and callback lifetime

Host `extern/aurora/lib/webgpu/gpu.cpp:979` installs an uncaptured-error callback that calls FATAL after initialization; device-loss callback does likewise. A non-null WebGPU handle does not prove successful shader/pipeline creation. Handle invalid WGSL, layout or pipeline errors before any object is used.

Recommended native C API sequence, to compile-check against the pinned Dawn generated header:

1. Push OutOfMemory, Internal and Validation scopes with `wgpuDevicePushErrorScope` on the same thread that performs creation.
2. Create shader, bind-group layout, pipeline layout and render pipeline synchronously. Do not publish them as ready yet.
3. Pop every scope in reverse order with `wgpuDevicePopErrorScope`, a `WGPUPopErrorScopeCallbackInfo` using `WGPUCallbackMode_WaitAnyOnly`, and durable result storage.
4. For each returned future initialize `WGPUFutureWaitInfo`; call `wgpuInstanceWaitAny(instance, 1, &wait, 0)` until completion. Only Success callback status plus NoError qualifies as success. Copy diagnostic message bytes while callback data remains valid.
5. Drain every callback before returning with stack userdata, failing mod initialization, releasing callback state, or unloading the library. A timeout is not cancellation. Avoid an unbounded tight-spin loop; a bounded initialization policy still needs a safe completion/drain path before unload.

`GfxDeviceInfo.instance` exists since GfxService 1.1. Use the borrowed instance, never create a second instance to wait on these futures. Do not replace host error/device-loss callbacks. Scopes cannot promise recovery from host-owned device loss.

Current upstream [Dawn API schema](https://raw.githubusercontent.com/google/dawn/main/src/dawn/dawn.json) supplies the future/callback signatures. Current [Dawn Device.cpp](https://raw.githubusercontent.com/google/dawn/main/src/dawn/native/Device.cpp) uses per-thread scope stacks and marks scope-pop events complete when no pending asynchronous tasks exist. These are supporting references; exact behavior must be checked against the host's pinned Dawn build. Synchronous creation should avoid async tasks, but a mod must still handle completion status.

Pipeline validation at initialization does not validate future bind groups or prevent allocation failure. Creating bind groups each frame with deterministic descriptors reduces logical validation risk but cannot guarantee graceful fallback from OOM. Scope and synchronously drain newly created resources, or cache validated objects with correctly retained snapshot lifetimes. Frame-only borrowed views cannot be retained as raw unowned pointers across frame boundaries. Do not claim complete GPU error fallback merely because shader/pipeline init uses scopes.

## Acceptance review still required

- Confirm the actual before-HUD callsite and its placement relative to fades and game particles.
- Verify enable=false records no snapshot/draw; unsupported layouts/MSAA bypass before mutation.
- Confirm worker payload lifetime and no game-service calls in draw callback.
- Compare before/after bytes and frame captures at stable camera/time, including nonopaque alpha.
- Exercise invalid shader, resize, changing render settings, unload/reload and Dawnlight coexistence.
- Distinguish callback execution count from successfully submitted GPU work; counters alone prove neither rendering nor parity.
- Capture ordinary HUD/menu/fades unaffected, and check no errors in host logs.

Until those checks execute, the correct status is a source-supported implementation candidate, not a completed visual or runtime milestone.
