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
    FrameClip clip;
};
struct Puff {
    Point center;
    float width, height, opacity, distance, kind;
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
        let surges = noise(vec2f(uv.x * 0.022, (in.world.y + time * 570.0) * 0.007));
        let streak = smoothstep(0.24, 0.83, ribbons * 0.53 + droplets * 0.19 + surges * 0.28);
        alpha = (0.13 + 0.64 * streak + 0.13 * broad) * smoothstep(0.0, 0.07, 1.0 - uv.y);
        tone = 0.60 + 0.40 * streak;
    } else if (in.data.z < 1.5) {
        let radius = length(uv);
        let turbulence = noise(uv * 13.0 + vec2f(time * 0.9, time * 0.55));
        let rings = 0.5 + 0.5 * sin(22.0 * radius - time * 7.0 + turbulence * 2.5);
        alpha = (1.0 - smoothstep(0.24, 1.0, radius)) * (0.30 + 0.48 * turbulence + 0.22 * rings);
        tone = 0.91 + turbulence * 0.09;
    } else if (in.data.z > 6.5) {
        let patches = noise(uv * vec2f(4.3, 5.7) + in.world.xz * 0.009 + vec2f(time * 0.28, -time * 0.37));
        let lace = noise(uv * 12.0 + in.world.xz * 0.018);
        alpha = smoothstep(0.43, 0.69, patches * 0.72 + lace * 0.28) *
                (1.0 - smoothstep(0.30, 1.0, length(uv)));
        tone = 0.94 + lace * 0.12;
    } else if (in.data.z < 2.5 || in.data.z > 5.5) {
        let fine = noise(uv * 5.2 + in.world.xz * 0.011 + vec2f(0.0, time * 0.42));
        let billow = noise(uv * 2.4 + in.world.xz * 0.006);
        let radius = length(uv);
        alpha = (1.0 - smoothstep(0.12, 1.0, radius + 0.30 * (0.5 - billow))) * (0.54 + 0.46 * fine);
        tone = 0.86 + fine * 0.14;
    } else if (in.data.z < 3.5) {
        let flow = in.world.y + time * 390.0;
        let breaks = noise(vec2f(uv.x * 4.2 + uv.y, flow * 0.026));
        let reach = 0.22 + 0.48 * noise(vec2f(uv.y * 2.3, flow * 0.013));
        let fringe = (1.0 - smoothstep(reach * 0.35, reach, uv.x)) * (0.42 + 0.58 * breaks);
        let thread = smoothstep(0.57, 0.85, breaks) *
                     smoothstep(reach * 0.7, reach, uv.x) * (1.0 - smoothstep(0.60, 1.0, uv.x));
        alpha = (fringe * 0.76 + thread * 0.47) * (1.0 - smoothstep(0.91, 1.0, uv.x));
        tone = 0.76 + 0.23 * breaks;
    } else if (in.data.z < 4.5) {
        let radius = length(uv);
        alpha = 1.0 - smoothstep(0.23, 1.0, radius);
        tone = 1.14;
    } else {
        // Dense aerated core at the contact line with turbulent, feathered edges.
        let billow = noise(uv * vec2f(5.0, 3.6) + in.world.xz * 0.006 + vec2f(-time * 0.78, time * 1.45));
        let peaks = noise(vec2f(in.world.x * 0.010 + in.world.z * 0.013, time * 1.1));
        let height = uv.y / mix(0.58, 1.0, peaks);
        let radius = length(vec2f(uv.x, mix(uv.y, height, smoothstep(0.0, 0.40, uv.y))));
        alpha = 1.0 - smoothstep(0.34, 0.97, radius + (billow - 0.5) * 0.36);
        tone = 0.80 + billow * 0.24;
    }
    let color = select(light, mix(light, vec3f(0.91, 0.94, 0.95), 0.36),
                       in.data.z > 4.5 && in.data.z < 5.5);
    return vec4f(color * tone, clamp(alpha * in.data.w, 0.0, 0.99));
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
    const auto& target = ctx->layout.color_attachments[GFX_SCENE_COLOR_ATTACHMENT_INDEX];
    const auto x = static_cast<unsigned>(std::ceil(draw.clip.left * target.width));
    const auto y = static_cast<unsigned>(std::ceil(draw.clip.top * target.height));
    const auto right = static_cast<unsigned>(draw.clip.right * target.width);
    const auto bottom = static_cast<unsigned>(draw.clip.bottom * target.height);
    if (right <= x || bottom <= y) return;
    if (draw.clip.left > 0 || draw.clip.top > 0 || draw.clip.right < 1 || draw.clip.bottom < 1)
        wgpuRenderPassEncoderSetScissorRect(ctx->pass, x, y, right - x, bottom - y);
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

static void collect_component(std::span<const Triangle> sheet, std::vector<Fall>& falls,
                              std::span<const Triangle> basin) {
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
    foot.y = low.y;
    dBgS_WtrChk water;
    const float reach = std::clamp(height * 0.28f, 90.0f, 460.0f);
    cXyz probe(foot.x, foot.y - reach, foot.z);
    water.Set(probe, foot.y + reach);
    if (dComIfG_Bgsp().WaterChk(&water)) foot.y = water.GetHeight();
    float nearest = 2500.0f * 2500.0f;
    for (const auto& tri : basin) {
        if (std::abs(tri[0].y - low.y) > 600.0f) continue;
        const float min_x = std::min({tri[0].x, tri[1].x, tri[2].x});
        const float max_x = std::max({tri[0].x, tri[1].x, tri[2].x});
        const float min_z = std::min({tri[0].z, tri[1].z, tri[2].z});
        const float max_z = std::max({tri[0].z, tri[1].z, tri[2].z});
        const float dx = foot.x - std::clamp(foot.x, min_x, max_x);
        const float dz = foot.z - std::clamp(foot.z, min_z, max_z);
        const float dy = tri[0].y - low.y;
        const float score = dx*dx + dz*dz + dy*dy;
        if (score >= nearest) continue;
        nearest = score;
        foot.y = (tri[0].y + tri[1].y + tri[2].y) / 3.0f;
    }
    Fall fall{{sheet.begin(), sheet.end()}, {}, foot, across, width, height, low.y};
    // Trace the actual sides at each height, including bends in the authored sheet.
    constexpr unsigned Samples = 40;
    for (unsigned i = 0; i <= Samples; ++i) {
        const float y = std::lerp(low.y, high.y, float(i) / Samples);
        std::array<Point, 2> edge{};
        float left = 1.0e9f, right = -left;
        for (const auto& tri : sheet) {
            for (unsigned j = 0; j < 3; ++j) {
                const auto a = tri[j], b = tri[(j + 1) % 3];
                if (a.y == b.y || y < std::min(a.y, b.y) || y > std::max(a.y, b.y)) continue;
                const float t = (y - a.y) / (b.y - a.y);
                const Point p{std::lerp(a.x, b.x, t), y, std::lerp(a.z, b.z, t)};
                const float u = p.x * across.x + p.z * across.z;
                if (u < left) { left = u; edge[0] = p; }
                if (u > right) { right = u; edge[1] = p; }
            }
        }
        if (left <= right) fall.edges.push_back(edge);
    }
    falls.push_back(std::move(fall));
}

void collect(std::span<const Triangle> sheet, std::vector<Fall>& falls,
             std::span<const Triangle> basin) {
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
        collect_component(component, falls, basin);
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

void draw(std::span<const Fall* const> falls, const CameraInfo& camera, const FrameClip* contact_clip) {
    if (!draw_type || falls.empty() || !settings::enhanced_water_enabled()) return;
    const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    const Point right{camera.view_from_world[0], camera.view_from_world[4], camera.view_from_world[8]};
    const Point up{camera.view_from_world[1], camera.view_from_world[5], camera.view_from_world[9]};
    const auto& wind = g_env_light.global_wind_influence;
    const float wind_length = std::hypot(wind.vec.x, wind.vec.z);
    const Point drift_wind = std::isfinite(wind_length) && wind_length > 0.001f && std::isfinite(wind.pow)
        ? Point{wind.vec.x / wind_length * std::clamp(wind.pow, 0.0f, 1.0f), 0,
                wind.vec.z / wind_length * std::clamp(wind.pow, 0.0f, 1.0f)} : Point{};
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
        const float scale_y = std::clamp(fall->height * 0.44f, 102.0f, 360.0f);
        if (!contact_clip) {
            for (const auto& tri : fall->sheet)
                for (auto p : tri)
                    vertices.push_back({add(p, scale(outward, 1.6f)), p.x*fall->across.x + p.z*fall->across.z,
                                        (p.y-fall->bottom)/fall->height, 0.0f, 0.90f});
            const float fringe_width = std::clamp(fall->width * 0.26f, 16.0f, 93.0f);
            for (unsigned edge = 0; edge < 2; ++edge) {
                const float side_sign = edge == 0 ? -1.0f : 1.0f;
                auto fringe_vertex = [&](Point p, bool outer) {
                    const float level = std::clamp((p.y - fall->bottom) / fall->height, 0.0f, 1.0f);
                    const float width = fringe_width * std::lerp(1.0f, 0.36f, level);
                    const Point offset = scale(fall->across, side_sign * (outer ? width : -width * 0.25f));
                    return Vertex{add(add(p, offset), scale(outward, 2.2f)), outer ? 1.0f : 0.0f,
                                  float(edge), 3.0f, std::min(1.0f, (1.0f - level) * 8.0f)};
                };
                for (size_t j = 1; j < fall->edges.size(); ++j) {
                    const auto a = fall->edges[j - 1][edge], b = fall->edges[j][edge];
                    const auto inner_a = fringe_vertex(a, false), outer_a = fringe_vertex(a, true);
                    const auto inner_b = fringe_vertex(b, false), outer_b = fringe_vertex(b, true);
                    vertices.insert(vertices.end(), {inner_a, outer_a, inner_b, inner_b, outer_a, outer_b});
                }
                if (fall->edges.size() < 2) continue;
                // Falling aerosol follows the traced silhouette and thickens on the way down.
                const unsigned count = std::clamp(unsigned(fall->height / 145.0f) + 3, 5u, 24u);
                for (unsigned j = 0; j < count && puffs.size() < 2200; ++j) {
                    const float seed = j * 3.471f + edge * 8.134f + fall->foot.x * 0.0011f;
                    const float phase = float(j) / count + time * (0.10f + 0.021f * std::sin(seed));
                    const float age = phase - std::floor(phase);
                    const float row = (1.0f - age) * (fall->edges.size() - 1);
                    const size_t lo = std::min(size_t(row), fall->edges.size() - 2);
                    const float t = row - float(lo);
                    const auto a = fall->edges[lo][edge], b = fall->edges[lo + 1][edge];
                    const Point anchor{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
                    const float spread = scale_y * (0.13f + age * 0.22f);
                    const float fade = std::min({1.0f, age * 12.0f, (1.0f - age) * 13.0f});
                    const auto center = add(anchor, add(scale(fall->across, side_sign * spread * (0.33f + 0.22f * std::sin(seed + time))),
                        add(scale(outward, spread * 0.45f), scale(drift_wind, spread * age))));
                    puffs.push_back({center, spread * 1.04f, std::clamp(fall->height / count * 0.82f, 25.0f, 210.0f),
                                     fade * std::lerp(0.15f, 0.45f, age), distance2(center), 2.0f});
                }
                // Concentrate short-lived spray where the silhouette changes direction.
                for (size_t row = 3; row + 3 < fall->edges.size(); row += 3) {
                    const auto a = fall->edges[row - 2][edge], p = fall->edges[row][edge],
                               b = fall->edges[row + 2][edge];
                    const auto incoming = add(p, scale(a, -1.0f));
                    const auto outgoing = add(b, scale(p, -1.0f));
                    if (incoming.y <= 0 || outgoing.y <= 0) continue;
                    const float bend = std::hypot(incoming.x / incoming.y - outgoing.x / outgoing.y,
                                                  incoming.z / incoming.y - outgoing.z / outgoing.y);
                    if (bend < 0.27f) continue;
                    for (unsigned j = 0; j < 5 && puffs.size() < 2200; ++j) {
                        const float seed = float(row) * 0.73f + j * 1.491f + edge * 2.11f;
                        const float phase = seed + time * (1.5f + 0.43f * std::sin(seed));
                        const float age = phase - std::floor(phase);
                        const float travel = scale_y * age * (0.22f + 0.48f * std::abs(std::cos(seed)));
                        const auto center = add(p, add(scale(fall->across, side_sign * travel),
                            add(scale(outward, travel * 0.55f), {0, scale_y * age * (0.25f - age * 0.76f), 0})));
                        const float radius = std::clamp(scale_y * 0.012f, 1.4f, 5.0f);
                        puffs.push_back({center, radius, radius * 2.2f, (1.0f - age) * 0.78f, distance2(center), 4.0f});
                    }
                }
            }
        }
        const unsigned emitters = std::clamp(unsigned(fall->width / 105.0f) + 1, 3u, 15u);
        for (unsigned i = 0; i < emitters; ++i) {
            const float spacing = fall->width / emitters;
            const Point foot = add(fall->foot, scale(fall->across, (i + 0.5f - emitters * 0.5f) * spacing));
            const Point impact = add(foot, add(scale(outward, scale_y * 0.35f), {0, 7, 0}));
            if (contact_clip) {
                const float seed = i * 2.39f + fall->foot.x * 0.0071f;
                const float billow = 0.5f + 0.31f * std::sin(time * 2.4f + seed) +
                                     0.19f * std::sin(time * 3.8f - seed * 0.67f);
                for (unsigned j = 0; j < 4; ++j) {
                    const float phase = time * (0.13f + 0.019f * j) + seed * 0.39f + j * 0.29f;
                    const float age = phase - std::floor(phase);
                    const float spread = scale_y * (0.24f + age * 2.85f);
                    const auto foam = add(foot, add(scale(outward, spread),
                        add(scale(fall->across, std::sin(seed + j * 4.13f) * spread * 0.72f), {0, 5, 0})));
                    const float opacity = std::min(1.0f, age * 12.0f) * (1.0f - age) * 0.74f;
                    quad(foam, scale(fall->across, spacing * 0.58f + scale_y * (0.21f + 0.39f * age)),
                         scale(outward, scale_y * (0.14f + 0.42f * age)), 7.0f, opacity);
                }
                // A low continuous core hides the join, with taller intermittent surges above it.
                const auto core = add(foot, add(scale(outward, scale_y * 0.93f), {0, scale_y * 0.13f, 0}));
                quad(core, scale(right, spacing * 0.95f + scale_y * 0.42f),
                     scale(up, scale_y * 0.33f), 5.0f, 1.0f);
                const auto skirt = add(foot, add(scale(outward, scale_y * 0.93f),
                                                {0, scale_y * (0.18f + 0.47f * billow), 0}));
                quad(skirt, scale(right, spacing * 0.63f + scale_y * (0.23f + 0.29f * billow)),
                     scale(up, scale_y * (0.26f + 0.64f * billow)), 5.0f, 0.37f + 0.49f * billow);
                // Settling vapor travels along the pool, expands, and thins with age.
                for (unsigned j = 0; j < 7 && puffs.size() < 2200; ++j) {
                    const float vapor_seed = i * 2.793f + j * 1.173f + fall->foot.z * 0.0019f;
                    const float phase = vapor_seed + time * (0.17f + 0.04f * std::sin(vapor_seed));
                    const float age = phase - std::floor(phase);
                    const float angle = vapor_seed + std::floor(phase) * 2.39996f;
                    const float travel = scale_y * (0.53f + 3.8f * age);
                    const auto lateral = scale(fall->across, std::sin(angle) * travel * 0.88f);
                    const auto forward = scale(outward, (0.40f + 0.60f * std::abs(std::cos(angle))) * travel);
                    const auto center = add(foot, add(add(lateral, forward),
                        add(scale(drift_wind, travel * 0.88f), {0, scale_y * (0.11f + 0.07f * (1.0f - age)), 0})));
                    const float fade = std::min(1.0f, age * 8.0f) * (1.0f - age);
                    puffs.push_back({center, (scale_y * 0.62f + spacing * 0.30f) * (0.56f + age),
                                     scale_y * (0.13f + age * 0.06f), 0.47f * fade, distance2(center), 6.0f});
                }
                continue;
            }
            quad(impact, scale(fall->across, spacing * 1.03f), scale(outward, scale_y * 1.07f), 1.0f, 0.94f);
            const auto veil = add(impact, {0, scale_y * 0.21f, 0});
            quad(veil, scale(right, spacing * 0.85f + scale_y * 0.40f),
                 scale(up, scale_y * 0.53f), 2.0f, 0.72f);
            for (unsigned j = 0; j < 11 && puffs.size() < 2200; ++j) {
                const float seed = float(i * 17 + j * 7) * 0.613f + fall->foot.x * 0.00073f;
                const float phase = time * (0.43f + 0.07f * j) + seed;
                const float age = phase - std::floor(phase);
                const float fade = std::min(1.0f, age * 14.0f) * (1.0f-age*age);
                const float drift = std::sin(seed + std::floor(phase) * 2.8f);
                const auto center = add(impact, add(scale(fall->across, drift * spacing * (0.20f + age * 0.48f)),
                    add(scale(outward, scale_y * age * 0.85f), {0, scale_y * (0.08f + age * 1.18f), 0})));
                puffs.push_back({center, (spacing * 0.61f + scale_y * 0.36f) * (0.63f + age),
                                 scale_y * (0.28f + age * 0.49f), fade * 0.68f, distance2(center), 2.0f});
            }
            for (unsigned j = 0; j < 17 && puffs.size() < 2200; ++j) {
                const float seed = i * 11.13f + j * 3.731f + fall->foot.z * 0.0017f;
                const float phase = time * (1.2f + 0.29f * std::sin(seed)) + seed;
                const float age = phase - std::floor(phase);
                const float angle = seed + std::floor(phase) * 2.39996f;
                const float flight = scale_y * age * (0.35f + 0.64f * std::abs(std::sin(angle)));
                const float rise = scale_y * age * (1.15f - age) * (0.8f + 1.18f * std::abs(std::cos(angle)));
                const auto center = add(impact, add(scale(fall->across, std::cos(angle) * flight),
                    add(scale(outward, flight * (0.35f + 0.65f * std::abs(std::sin(angle)))), {0, rise + 9.0f, 0})));
                const float radius = std::clamp(scale_y * (0.010f + 0.012f * std::abs(std::sin(seed))), 1.5f, 8.0f);
                puffs.push_back({center, radius, radius * (1.5f + std::abs(1.0f-2.0f*age)),
                                 0.93f * (1.0f - age*age), distance2(center), 4.0f});
            }
        }
    }
    std::sort(puffs.begin(), puffs.end(), [](const auto& a, const auto& b) { return a.distance > b.distance; });
    for (const auto& puff : puffs)
        quad(puff.center, scale(right, puff.width), scale(up, puff.height), puff.kind, puff.opacity);
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
    if (contact_clip && !twilight && !g_env_light.hide_vrbox) {
        const auto sky = g_env_light.vrbox_sky_col;
        if (sky.r || sky.g || sky.b) {
            const auto tint = [](float channel) { return std::clamp(channel / 255.0f, 0.0f, 1.0f); };
            uniforms.red = std::lerp(uniforms.red, tint(sky.r), 0.25f);
            uniforms.green = std::lerp(uniforms.green, tint(sky.g), 0.25f);
            uniforms.blue = std::lerp(uniforms.blue, tint(sky.b), 0.25f);
        }
    }
    Draw draw{{}, {}, static_cast<unsigned>(vertices.size()), contact_clip ? *contact_clip : FrameClip{0, 0, 1, 1}};
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &draw.uniforms) == MOD_OK &&
        svc_gfx->push_verts(mod_ctx, vertices.data(), vertices.size() * sizeof(Vertex), 4, &draw.vertices) == MOD_OK)
        (void)svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw));
}
} // namespace midnafx::render::waterfall
