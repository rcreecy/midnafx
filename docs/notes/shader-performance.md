# Shader and performance research / proposed milestones

Pins: Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`; nested Aurora `7f2801cd0133c9333eadb4e2e6b24100c328d328`. Paths below are relative to `upstream/dusklight`. This is a design proposal and source review, not a measured performance result or production shader.

## Supported shader path and pass costs

`sdk/include/mods/svc/gfx.h` exposes GfxService 1.2 and raw WebGPU C handles; `FEATURES webgpu` is mandatory to include it. WGSL is the demonstrated portable source format: `mods/ao_mod/res/composite.wgsl` and `mods/ao_mod/src/mod.cpp` use a fullscreen triangle and cached pipeline. Native backend shader translation is the host/WebGPU implementation's responsibility; MidnaFX should not ship a Metal-only path or compile variants per slider adjustment.

`GFX_STAGE_FRAME_BEFORE_HUD` is the candidate grading hook, whose ordering must be verified against painter evidence in rendering research. Stage callbacks run on the game thread; draw callbacks run on the render worker and cannot call ConfigService, UiService, LogService or other services. Transfer validated immutable scalar snapshots through `push_uniform`/`push_draw`; do not read mutable UI globals from a draw callback. Payload cap is 128 bytes. `GfxResolvedTargets` views are borrowed for the current frame only.

`extern/aurora/lib/gfx/recording.cpp::resolve_pass` seals the current EFB pass and resumes an EFB loading continuation. Color snapshots acquire a pooled texture and set `snapshotColorDst`. `extern/aurora/lib/gfx/encoding.cpp` then records `CopyTextureToTexture` under the profiling zone `Pass snapshot`. Thus one fullscreen shader pass is not a zero-copy operation: account for pass break, resolve when MSAA is active, full image copy, image read, output write, and continuation attachment load/store. Do not call `create_pass` for the initial grade: the resolved continuation already supplies the output target. Request color only, no depth.

Configure all color targets using `gfx_init_color_target_states`; only scene color (attachment 0) may be writable. Disable blending, depth writes, culling and stencil modification. Pipeline depth format, MSAA sample count, attachment count and formats must match `GfxDrawContext.layout`. Cache pipelines using layout key and recreate only when layout changes. Set full-target viewport/scissor in the custom draw (host promises restoration afterward). A disabled or neutral grade should return before resolving so normal bypass adds no GPU pass/copy.

## Milestone 1 passthrough and pixel coordinates

Proposed WGSL sketch:

```wgsl
@group(0) @binding(0) var scene: texture_2d<f32>;
@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
    return vec4f(uv * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), 0.0, 1.0);
}
@fragment
fn fs_main(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let last = vec2i(textureDimensions(scene)) - vec2i(1);
    let pixel = clamp(vec2i(position.xy), vec2i(0), last);
    return textureLoad(scene, pixel, 0);
}
```

The triangle coordinate convention follows `mods/ao_mod/res/composite.wgsl::vs_main`: top-left clip (-1,+1) maps to UV (0,0), so no additional Y flip. The host's own `extern/aurora/lib/webgpu/gpu.cpp::resampleShaderSource`, `sample_area` and `sample_by_pixel` show fragment-position-to-texture mapping, pixel-center ±0.5 bounds and clamped textureLoad. Using integer fragment positions for same-resolution source and target avoids filtering, normalized-UV interpolation and sampler creation. Validate snapshot dimensions equal output attachment dimensions; skip/report mismatch rather than silently applying incorrect scaling. Capture corner markers and a checkerboard in the real host to verify orientation and one-to-one sampling.

Preserve the entire sampled vec4 for passthrough, and sample alpha unchanged when grading RGB. Do not copy the host final presentation shader's `vec4(color.rgb, 1.0)` behavior into an earlier scene pass: that shader is a final output operation and is not evidence that scene alpha is disposable. RGB-only write mask can alternatively preserve destination alpha, but retaining sampled alpha is the clearest initial passthrough semantics. Auxiliary attachments and scene depth must remain unchanged.

Passthrough cannot be promised bit-exact under all host settings: snapshot resolve and writing back to multisampled attachments collapses per-sample color variation. Test MSAA off and all supported enabled levels; inspect HUD-adjacent edges and any subsequent transparent composition. Bypass comparison is mandatory before introducing grading.

## Color domain and fused grading proposal

Aurora `extern/aurora/lib/webgpu/gpu.cpp::best_surface_format` prefers RGBA8Unorm/BGRA8Unorm after `to_linear` removes sRGB format suffix. This is evidence about surface selection only; an Unorm format does not prove physically linear scene values. Inspect the scene format at runtime and defer unconditional sRGB decode/encode until host gamma/TEV/frame-copy research establishes its meaning. Never silently add a second gamma conversion in the identity path. Reject unsupported layouts gracefully.

Start with one scene textureLoad, arithmetic and one RGB output, preserving alpha. CPU-side validation builds a compact uniform block and precomputes exposure multiplier (exp2), reciprocal gamma and a combined affine RGB transform for compatible exposure/contrast/black-offset/channel-balance operations. Saturation may be folded into a matrix when its order is fixed; vibrance is color-dependent and stays shader arithmetic. A cheap shoulder and gamma require nonlinear arithmetic; guard negative input to pow and use an identity branch for neutral gamma. Clamp only where the selected documented color domain requires it; an early clamp can irreversibly discard highlight information.

A 3D LUT is not justified initially: it adds texture storage, upload/state management, interpolation/sample cost and precision considerations for a small analytic operator set. A fused analytic shader keeps one scene read. Benchmark before adopting a LUT for complex future grading. Optional sharpening adds neighborhood reads (e.g. center plus four neighbors) and therefore requires a separate feature variant/cost accounting; keep it off by default and out of M1. Do not add sharpening reads unconditionally with zero strength.

## Performance accounting and validation

At 3840x2160 there are 8,294,400 pixels. For four-byte scene color, one full image is 33,177,600 bytes (~31.64 MiB). A simple lower-order traffic estimate for snapshot read+write and grade read+write is 132,710,400 bytes/frame (~126.56 MiB), about 7.96 GB/s at 60 fps. This is an explanatory estimate, not measured external-memory traffic: caching, compression, MSAA, attachment load/store, GPU architecture and later host work change it. It demonstrates why bypass and avoiding extra passes matter on an Intel Mac.

The public GfxService exposes no direct timing API. Aurora internally has `extern/aurora/lib/webgpu/gpu_prof.hpp::{frame_begin,frame_end,pass_writes,Zone}` and encoding creates pass timestamp writes, but these are not an SDK service contract. CPU clock around a stage callback measures recording overhead, not GPU execution. Prefer existing host profiling or platform GPU tools for real elapsed GPU cost. Do not inject synchronous readbacks or queue waits into production.

Acceptance matrix: disabled baseline; forced passthrough; neutral grade bypass; nonneutral grade; 1080p/4K; MSAA off/on; resize; enable/disable/reload; Dawnlight off/on; HUD unaffected; preset churn while rendering; Intel x86_64 Mac native backend. Separate startup pipeline compilation time from warmed steady state. Report median and high-percentile CPU/GPU frame times, sample duration, actual GPU/backend, resolution and graphics settings. No measured frame-time claim is valid until those runs exist.
