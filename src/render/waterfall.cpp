#include "waterfall.hpp"
#include "services.hpp"
#include "ui/settings.hpp"

#include <d/d_kankyo.h>
#include <d/d_com_inf_game.h>
#include <d/d_bg_s_wtr_chk.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace midnafx::render::waterfall {
namespace {
struct Vertex {
    Point position;
    float u, v, kind, opacity;
};
struct Uniforms {
    float projection[16];
    float time, red, green, blue;
};
struct Draw {
    GfxRange vertices, uniforms;
    unsigned count;
};
struct Puff {
    Point center;
    float width, height, opacity, distance;
};
GfxDrawTypeHandle draw_type = 0;
WGPURenderPipeline pipeline = nullptr;
std::uint64_t layout_key = 0;
bool reversed = false;
std::vector<Vertex> vertices;
std::vector<Puff> puffs;
auto start = std::chrono::steady_clock::now();

constexpr char Shader[] = R"(
struct Uniforms {
    projection: mat4x4f,
    time: f32,
    red: f32,
    green: f32,
    blue: f32,
}
@group(0) @binding(0) var<uniform> scene: Uniforms;
struct Output {
    @builtin(position) position: vec4f,
    @location(0) world: vec3f,
    @location(1) data: vec4f,
}
@vertex fn vs_main(@location(0) point: vec3f, @location(1) data: vec4f) -> Output {
    return Output(scene.projection * vec4f(point, 1.0), point, data);
}
fn hash(p: vec2f) -> f32 {
    let q = fract(p * vec2f(0.1031, 0.11369));
    return fract((q.x + q.y) * (19.19 + dot(q, q)) * 13.17);
}
fn noise(p: vec2f) -> f32 {
    let cell = floor(p);
    let f = fract(p);
    let u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(cell), hash(cell + vec2f(1, 0)), u.x),
               mix(hash(cell + vec2f(0, 1)), hash(cell + vec2f(1, 1)), u.x), u.y);
}
@fragment fn fs_main(in: Output) -> @location(0) vec4f {
    let uv = in.data.xy;
    let light = vec3f(scene.red, scene.green, scene.blue);
    let time = scene.time;
    var alpha: f32;
    var tone: f32;
    if (in.data.z < 0.5) {
        // Positive time on world Y advects the detail downward.
        let broad = noise(vec2f(uv.x * 0.041, (in.world.y + time * 315.0) * 0.012));
        let ribbons = noise(vec2f(uv.x * 0.14 + broad * 1.5, (in.world.y + time * 430.0) * 0.026));
        let droplets = noise(vec2f(uv.x * 0.29, (in.world.y + time * 560.0) * 0.055));
        let streak = smoothstep(0.29, 0.87, ribbons * 0.70 + droplets * 0.30);
        alpha = (0.10 + 0.49 * streak + 0.11 * broad) * smoothstep(0.0, 0.07, 1.0 - uv.y);
        tone = 0.69 + 0.29 * streak;
    } else if (in.data.z < 1.5) {
        let radius = length(uv);
        let turbulence = noise(uv * 13.0 + vec2f(time * 0.9, time * 0.55));
        let rings = 0.5 + 0.5 * sin(22.0 * radius - time * 7.0 + turbulence * 2.5);
        alpha = (1.0 - smoothstep(0.24, 1.0, radius)) * (0.30 + 0.48 * turbulence + 0.22 * rings);
        tone = 0.91 + turbulence * 0.09;
    } else {
        let fine = noise(uv * 5.2 + in.world.xz * 0.011 + vec2f(0.0, time * 0.42));
        let billow = noise(uv * 2.4 + in.world.xz * 0.006);
        let radius = length(uv);
        alpha = (1.0 - smoothstep(0.12, 1.0, radius + 0.30 * (0.5 - billow))) * (0.54 + 0.46 * fine);
        tone = 0.86 + fine * 0.14;
    }
    return vec4f(light * tone, clamp(alpha * in.data.w, 0.0, 0.85));
}
)";

Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Point scale(Point p, float s) { return {p.x * s, p.y * s, p.z * s}; }

void quad(Point center, Point right, Point up, float kind, float opacity) {
    constexpr float corners[][2] = {{-1,-1}, {1,-1}, {-1,1}, {-1,1}, {1,-1}, {1,1}};
    for (const auto& uv : corners)
        vertices.push_back({add(center, add(scale(right, uv[0]), scale(up, uv[1]))),
                            uv[0], uv[1], kind, opacity});
}

void encode(ModContext*, const GfxDrawContext* ctx, const void* payload, size_t size, void*) {
    if (size != sizeof(Draw) || !ctx->layout.color_attachment_count ||
        ctx->layout.depth_stencil_format == WGPUTextureFormat_Undefined) return;
    if (!pipeline || layout_key != ctx->layout.key || reversed != ctx->uses_reversed_z) {
        if (pipeline) wgpuRenderPipelineRelease(pipeline);
        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
        source.code = {Shader, WGPU_STRLEN};
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
        blend.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha};
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
        desc.label = {"MidnaFX waterfalls", WGPU_STRLEN};
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
} // namespace

static void collect_component(std::span<const Triangle> sheet, std::vector<Fall>& falls) {
    if (sheet.empty() || sheet.size() > 2048 || falls.size() >= 12) return;
    Point low = sheet[0][0], high = low;
    Point horizontal{};
    for (const auto& tri : sheet) {
        const auto a = add(tri[1], scale(tri[0], -1.0f));
        const auto b = add(tri[2], scale(tri[0], -1.0f));
        Point edge{a.x * b.y - a.y * b.x, 0.0f, a.z * b.y - a.y * b.z};
        if (edge.x * horizontal.x + edge.z * horizontal.z < 0.0f) edge = scale(edge, -1.0f);
        horizontal = add(horizontal, edge);
        for (auto p : tri) {
            low = {std::min(low.x,p.x), std::min(low.y,p.y), std::min(low.z,p.z)};
            high = {std::max(high.x,p.x), std::max(high.y,p.y), std::max(high.z,p.z)};
        }
    }
    const float height = high.y - low.y;
    const float length = std::hypot(horizontal.x, horizontal.z);
    if (height < 45.0f || length < 1.0f) return;
    const Point across = scale(horizontal, 1.0f / length);
    float min_u = 1.0e9f, max_u = -1.0e9f;
    Point foot{};
    unsigned count = 0;
    for (const auto& tri : sheet) {
        for (auto p : tri) {
            if (p.y > low.y + height * 0.16f) continue;
            foot = add(foot, p);
            ++count;
            const float u = p.x * across.x + p.z * across.z;
            min_u = std::min(min_u, u);
            max_u = std::max(max_u, u);
        }
    }
    const float width = max_u - min_u;
    if (!count || width < 16.0f) return;
    foot = scale(foot, 1.0f / count);
    foot = add(foot, scale(across, 0.5f * (min_u + max_u) - foot.x * across.x - foot.z * across.z));
    foot.y = low.y + std::clamp(height * 0.04f, 3.0f, 30.0f);
    dBgS_WtrChk water;
    const float reach = std::clamp(height * 0.28f, 90.0f, 240.0f);
    cXyz probe(foot.x, foot.y - reach, foot.z);
    water.Set(probe, foot.y + reach);
    if (dComIfG_Bgsp().WaterChk(&water)) foot.y = std::max(foot.y, water.GetHeight() + 4.0f);
    falls.push_back({{sheet.begin(), sheet.end()}, foot, across, width, height, low.y});
}

void collect(std::span<const Triangle> sheet, std::vector<Fall>& falls) {
    if (sheet.empty() || sheet.size() > 2048) return;
    // Authoring often packs several disconnected tiers into one material.
    // Group coincident edges before estimating each impact line.
    std::vector<bool> used(sheet.size());
    for (size_t seed = 0; seed < sheet.size() && falls.size() < 12; ++seed) {
        if (used[seed]) continue;
        std::vector<Triangle> component{sheet[seed]};
        used[seed] = true;
        for (size_t at = 0; at < component.size(); ++at) {
            const auto current = component[at];
            for (size_t candidate = seed + 1; candidate < sheet.size(); ++candidate) {
                if (used[candidate]) continue;
                unsigned shared = 0;
                for (const auto& p : sheet[candidate]) {
                    shared += std::any_of(current.begin(), current.end(), [&](Point q) {
                        return std::abs(p.x-q.x) < 0.1f && std::abs(p.y-q.y) < 0.1f &&
                               std::abs(p.z-q.z) < 0.1f;
                    });
                }
                if (shared < 2) continue;
                component.push_back(sheet[candidate]);
                used[candidate] = true;
            }
        }
        collect_component(component, falls);
    }
}

void initialize() {
    if (!svc_gfx || !svc_gfx->push_uniform || !svc_camera || !svc_camera->get_camera) return;
    GfxDrawTypeDesc desc = GFX_DRAW_TYPE_DESC_INIT;
    desc.label = "MidnaFX waterfalls";
    desc.draw = encode;
    if (svc_gfx->register_draw_type(mod_ctx, &desc, &draw_type) != MOD_OK) draw_type = 0;
    start = std::chrono::steady_clock::now();
}

void shutdown() {
    if (draw_type) svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    draw_type = 0;
    if (pipeline) wgpuRenderPipelineRelease(pipeline);
    pipeline = nullptr;
    std::vector<Vertex>().swap(vertices);
    std::vector<Puff>().swap(puffs);
}

void draw(std::span<const Fall* const> falls, const CameraInfo& camera) {
    if (!draw_type || falls.empty() || !settings::enhanced_water_enabled()) return;
    const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    const Point right{camera.view_from_world[0], camera.view_from_world[4], camera.view_from_world[8]};
    const Point up{camera.view_from_world[1], camera.view_from_world[5], camera.view_from_world[9]};
    const auto distance2 = [&](Point p) {
        const auto d = add(p, {-camera.eye[0], -camera.eye[1], -camera.eye[2]});
        return d.x*d.x + d.y*d.y + d.z*d.z;
    };
    vertices.clear();
    puffs.clear();
    for (const auto* fall : falls) {
        if (distance2(fall->foot) > 18000.0f * 18000.0f || vertices.size() > 42000) continue;
        const Point face{-fall->across.z, 0, fall->across.x};
        const float side = (camera.eye[0]-fall->foot.x)*face.x + (camera.eye[2]-fall->foot.z)*face.z;
        const Point outward = scale(face, side >= 0.0f ? 1.0f : -1.0f);
        for (const auto& tri : fall->sheet)
            for (auto p : tri)
                vertices.push_back({add(p, scale(outward, 1.6f)), p.x*fall->across.x + p.z*fall->across.z,
                                    (p.y-fall->bottom)/fall->height, 0.0f, 0.90f});
        const float scale_y = std::clamp(fall->height * 0.35f, 82.0f, 270.0f);
        const unsigned emitters = std::clamp(unsigned(fall->width / 130.0f) + 1, 2u, 13u);
        for (unsigned i = 0; i < emitters; ++i) {
            const float spacing = fall->width / emitters;
            const Point foot = add(fall->foot, scale(fall->across, (i + 0.5f - emitters * 0.5f) * spacing));
            const Point impact = add(foot, add(scale(outward, scale_y * 0.67f), {0, 4, 0}));
            quad(impact, scale(fall->across, spacing * 0.80f), scale(outward, scale_y * 0.92f), 1.0f, 0.79f);
            for (unsigned j = 0; j < 7; ++j) {
                const float seed = float(i * 17 + j * 7) * 0.613f + fall->foot.x * 0.00073f;
                const float phase = time * (0.43f + 0.07f * j) + seed;
                const float age = phase - std::floor(phase);
                const float fade = std::min(1.0f, age * 10.0f) * (1.0f-age);
                const float drift = std::sin(seed + std::floor(phase) * 2.8f);
                const auto center = add(impact, add(scale(fall->across, drift * spacing * (0.20f + age * 0.48f)),
                    add(scale(outward, scale_y * age * 0.68f), {0, scale_y * (0.15f + age * 1.25f), 0})));
                puffs.push_back({center, (spacing * 0.58f + scale_y * 0.29f) * (0.58f + age),
                                 scale_y * (0.24f + age * 0.45f), fade * 0.77f, distance2(center)});
            }
        }
    }
    std::sort(puffs.begin(), puffs.end(), [](const auto& a, const auto& b) { return a.distance > b.distance; });
    for (const auto& puff : puffs)
        quad(puff.center, scale(right, puff.width), scale(up, puff.height), 2.0f, puff.opacity);
    if (vertices.empty()) return;
    Uniforms uniforms{};
    std::memcpy(uniforms.projection, camera.proj_from_world, sizeof(uniforms.projection));
    uniforms.time = time;
    const auto ambient = g_env_light.bg_amb_col[0];
    const bool twilight = dKy_darkworld_check();
    const auto light = [](float value) { return std::clamp(0.60f + value / 640.0f, 0.60f, 1.0f); };
    uniforms.red = light(ambient.r) * (twilight ? 0.72f : 0.98f);
    uniforms.green = light(ambient.g) * (twilight ? 0.86f : 0.99f);
    uniforms.blue = light(ambient.b);
    Draw draw{{}, {}, static_cast<unsigned>(vertices.size())};
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &draw.uniforms) == MOD_OK &&
        svc_gfx->push_verts(mod_ctx, vertices.data(), vertices.size() * sizeof(Vertex), 4, &draw.vertices) == MOD_OK)
        (void)svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw));
}
} // namespace midnafx::render::waterfall
