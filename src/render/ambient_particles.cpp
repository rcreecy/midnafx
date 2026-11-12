#include "ambient_particles.hpp"
#include "airborne_motion.hpp"
#include "ground_grass.hpp"
#include "game/topology.hpp"
#include "game/water_probe.hpp"
#include "game/wind.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <d/d_com_inf_game.h>
#include <d/d_kankyo.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace midnafx::render::ambient_particles {
namespace {
using Point = topology::Vec3;
struct Vertex { Point point; float u, v, kind, opacity; };
struct Uniforms { float projection[16]; float tint[4]; };
struct Draw { GfxRange vertices, uniforms; unsigned count; };
constexpr unsigned MaxSprites = 192;
GfxStageHookHandle hook = 0;
GfxDrawTypeHandle draw_type = 0;
WGPURenderPipeline pipeline = nullptr;
std::uint64_t layout_key = 0;
bool reversed = false;
std::vector<Vertex> vertices;
std::string reported_stage;
bool reported_day = false, reported_night = false;
const auto started = std::chrono::steady_clock::now();
constexpr char Shader[] = R"(
struct Uniforms { projection: mat4x4f, tint: vec4f, }
@group(0) @binding(0) var<uniform> scene: Uniforms;
struct Output { @builtin(position) position: vec4f, @location(0) data: vec4f, }
@vertex fn vs_main(@location(0) point: vec3f, @location(1) data: vec4f) -> Output {
    return Output(scene.projection * vec4f(point, 1.0), data);
}
@fragment fn fs_main(in: Output) -> @location(0) vec4f {
    let radius2 = dot(in.data.xy, in.data.xy);
    let glow = exp(-radius2 * 7.0) * (1.0 - smoothstep(0.60, 1.0, sqrt(radius2)));
    let core = exp(-radius2 * 72.0);
    let tint = select(scene.tint.rgb, vec3f(0.82, 1.0, 0.34) * (1.0 + core), in.data.z > 0.5);
    return vec4f(tint, glow * in.data.w);
}
)";
void encode(ModContext*, const GfxDrawContext* ctx, const void* payload, size_t size, void*) {
    if (size != sizeof(Draw) || !ctx->layout.color_attachment_count ||
        ctx->layout.depth_stencil_format == WGPUTextureFormat_Undefined) return;
    if (!pipeline || layout_key != ctx->layout.key || reversed != ctx->uses_reversed_z) {
        if (pipeline) wgpuRenderPipelineRelease(pipeline);
        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
        const std::string shader_source = Shader;
        source.code = {shader_source.data(), shader_source.size()};
        WGPUShaderModuleDescriptor sd = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        sd.nextInChain = &source.chain;
        const auto shader = wgpuDeviceCreateShaderModule(ctx->device, &sd);
        WGPUVertexAttribute attrs[2]{};
        attrs[0].format = WGPUVertexFormat_Float32x3;
        attrs[1].format = WGPUVertexFormat_Float32x4;
        attrs[1].offset = offsetof(Vertex, u);
        attrs[1].shaderLocation = 1;
        WGPUVertexBufferLayout buffer = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        buffer.arrayStride = sizeof(Vertex);
        buffer.attributeCount = 2;
        buffer.attributes = attrs;
        WGPUBlendState blend{};
        blend.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_One};
        blend.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_Zero, WGPUBlendFactor_One};
        WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shader;
        fragment.entryPoint = {"fs_main", WGPU_STRLEN};
        fragment.targetCount = gfx_init_color_target_states(&ctx->layout, colors, &blend, WGPUColorWriteMask_All);
        fragment.targets = colors;
        WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
        depth.format = ctx->layout.depth_stencil_format;
        depth.depthWriteEnabled = WGPUOptionalBool_False;
        depth.depthCompare = ctx->uses_reversed_z ? WGPUCompareFunction_GreaterEqual : WGPUCompareFunction_LessEqual;
        WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX airborne particles", WGPU_STRLEN};
        desc.vertex.module = shader;
        desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
        desc.vertex.bufferCount = 1;
        desc.vertex.buffers = &buffer;
        desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        desc.multisample.count = ctx->layout.sample_count;
        desc.depthStencil = &depth;
        desc.fragment = &fragment;
        pipeline = wgpuDeviceCreateRenderPipeline(ctx->device, &desc);
        wgpuShaderModuleRelease(shader);
        layout_key = ctx->layout.key;
        reversed = ctx->uses_reversed_z;
    }
    if (!pipeline) return;
    Draw draw{};
    std::memcpy(&draw, payload, sizeof(draw));
    auto layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.buffer = ctx->uniform_buffer;
    entry.offset = draw.uniforms.offset;
    entry.size = draw.uniforms.size;
    WGPUBindGroupDescriptor binding = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    binding.layout = layout;
    binding.entryCount = 1;
    binding.entries = &entry;
    auto group = wgpuDeviceCreateBindGroup(ctx->device, &binding);
    wgpuBindGroupLayoutRelease(layout);
    if (!group) return;
    wgpuRenderPassEncoderSetPipeline(ctx->pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(ctx->pass, 0, ctx->vertex_buffer, draw.vertices.offset, draw.vertices.size);
    wgpuRenderPassEncoderDraw(ctx->pass, draw.count, 1, 0, 0);
    wgpuBindGroupRelease(group);
}

void stage(ModContext*, const GfxStageContext* context, void*) {
    if (!context || !context->game_view || !settings::atmosphere_enabled() || settings::atmosphere_strength() <= 0 ||
        !dComIfGp_getPlayer(0) || dComIfGp_isPauseFlag() || !dKy_Outdoor_check() ||
        dKy_darkworld_check() || dKy_camera_water_in_status_check()) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (!svc_camera || !svc_camera->get_camera ||
        svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK ||
        !std::all_of(std::begin(camera.proj_from_world), std::end(camera.proj_from_world),
                     [](float value) { return std::isfinite(value); }) ||
        !std::all_of(std::begin(camera.eye), std::end(camera.eye),
                     [](float value) { return std::isfinite(value) && std::abs(value) < 1e7f; })) return;
    const auto environment = water_probe::environment();
    const float night = airborne::night_strength(dComIfGs_getTime());
    const float day = environment.sun_color[3] * (1.0f-night);
    if (day < 0.01f && night < 0.01f) return;
    std::array<ground_grass::AirborneSource, 64> sources{};
    const auto count = ground_grass::airborne_sources(camera, sources);
    if (!count) return;
    const float time = std::chrono::duration<float>(std::chrono::steady_clock::now()-started).count();
    const auto wind = wind::sample();
    const Point right{camera.view_from_world[0], camera.view_from_world[4], camera.view_from_world[8]};
    const Point up{camera.view_from_world[1], camera.view_from_world[5], camera.view_from_world[9]};
    vertices.clear();
    unsigned pollen = 0, fireflies = 0;
    auto sprite = [&](const auto& source, std::uint32_t seed, bool firefly) {
        if (vertices.size() >= MaxSprites*6) return;
        const auto motion = firefly ? airborne::firefly(seed, time, wind) : airborne::pollen(seed, time, wind);
        const Point point{source.point[0]+motion.offset[0], source.point[1]+motion.offset[1], source.point[2]+motion.offset[2]};
        const float dx = point.x-camera.eye[0], dy = point.y-camera.eye[1], dz = point.z-camera.eye[2];
        const float distance = std::sqrt(dx*dx+dy*dy+dz*dz);
        const auto* m = camera.proj_from_world;
        const float w = m[3]*point.x+m[7]*point.y+m[11]*point.z+m[15];
        if (w <= 0 || std::abs(m[0]*point.x+m[4]*point.y+m[8]*point.z+m[12]) > w*1.1f ||
            std::abs(m[1]*point.x+m[5]*point.y+m[9]*point.z+m[13]) > w*1.1f) return;
        const float haze = std::clamp((distance-g_env_light.mFogNear) /
                            std::max(1.0f, g_env_light.mFogFar-g_env_light.mFogNear), 0.0f, 1.0f);
        const float fade = airborne::smooth(35, 120, distance) * (1.0f-airborne::smooth(1800, 2600, distance));
        const float strength = std::clamp(settings::atmosphere_strength() / 0.35f, 0.0f, 2.0f);
        const float alpha = motion.opacity * (firefly ? night*0.92f : day*0.42f) * fade * (1.0f-haze) * strength;
        if (alpha < 0.003f) return;
        const float size = firefly ? 4.5f+airborne::random(seed+7)*2.5f : 1.2f+airborne::random(seed+7)*1.1f;
        constexpr float corners[][2]={{-1,-1},{1,-1},{-1,1},{-1,1},{1,-1},{1,1}};
        for (const auto& uv : corners) {
            const Point p{point.x+(right.x*uv[0]+up.x*uv[1])*size,
                          point.y+(right.y*uv[0]+up.y*uv[1])*size,
                          point.z+(right.z*uv[0]+up.z*uv[1])*size};
            vertices.push_back({p, uv[0], uv[1], firefly ? 1.0f : 0.0f, alpha});
        }
        if (firefly) ++fireflies; else ++pollen;
    };
    for (unsigned i = 0; i < count; ++i) {
        const auto& source = sources[i];
        const auto seed = std::bit_cast<std::uint32_t>(source.point[0])*0x9e3779b9u ^
                          std::bit_cast<std::uint32_t>(source.point[2])*0x85ebca6bu;
        if (night > 0.01f && source.forest && airborne::random(seed) < 0.52f && fireflies < 32)
            sprite(source, seed, true);
        if (day > 0.01f) for (unsigned mote = 0; mote < 4; ++mote) sprite(source, seed+mote*0x7feb352du, false);
    }
    if (vertices.empty()) return;
    Uniforms uniforms{};
    std::memcpy(uniforms.projection, camera.proj_from_world, sizeof(uniforms.projection));
    for (unsigned i = 0; i < 3; ++i) uniforms.tint[i] = std::clamp(0.55f+environment.sun_color[i]*0.35f, 0.0f, 1.0f);
    Draw draw{{}, {}, static_cast<unsigned>(vertices.size())};
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &draw.uniforms) != MOD_OK ||
        svc_gfx->push_verts(mod_ctx, vertices.data(), vertices.size()*sizeof(Vertex), 4, &draw.vertices) != MOD_OK) return;
    (void)svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw));
    const auto* name = dComIfGp_getStartStageName();
    if (name && reported_stage != name) { reported_stage=name; reported_day=reported_night=false; }
    if ((pollen && !reported_day) || (fireflies && !reported_night)) {
        char message[128];
        std::snprintf(message, sizeof(message), "Airborne particles: %u pollen, %u fireflies; vegetation anchors, shared wind", pollen, fireflies);
        svc_log->info(mod_ctx, message);
        reported_day |= pollen != 0; reported_night |= fireflies != 0;
    }
}
} // namespace
void update() {
    if (!draw_type || settings::atmosphere_enabled() == (hook != 0)) return;
    if (!settings::atmosphere_enabled()) {
        svc_gfx->unregister_stage_hook(mod_ctx, hook); hook=0; return;
    }
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback=stage;
    (void)svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &desc, &hook);
}
void initialize() {
    if (!svc_gfx || !svc_camera || !svc_gfx->register_draw_type || !svc_gfx->register_stage_hook) return;
    GfxDrawTypeDesc desc = GFX_DRAW_TYPE_DESC_INIT;
    desc.label="MidnaFX airborne particles"; desc.draw=encode;
    if (svc_gfx->register_draw_type(mod_ctx, &desc, &draw_type) == MOD_OK) update();
}
void shutdown() {
    if (hook) svc_gfx->unregister_stage_hook(mod_ctx, hook);
    if (draw_type) svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    hook=draw_type=0;
    if (pipeline) wgpuRenderPipelineRelease(pipeline);
    pipeline=nullptr; layout_key=0;
    vertices.clear(); reported_stage.clear(); reported_day=reported_night=false;
}
}
