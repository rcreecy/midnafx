#include "ground_grass.hpp"
#include "game/topology.hpp"
#include "services.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModel.h>
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DTexture.h>
#include <JSystem/JUtility/JUTNameTab.h>
#include <m_Do/m_Do_ext.h>
#include <d/actor/d_a_bg.h>
#include <d/d_com_inf_game.h>
#include <d/d_resorce.h>
#include <f_op/f_op_actor_iter.h>
#include <f_op/f_op_actor_mng.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace midnafx::render::ground_grass {
namespace {
using topology::Vec3;
constexpr unsigned MaxRoots = 120000;
constexpr unsigned MaxVisible = 6500;
constexpr float DrawDistance = 3400.0f;

struct Root {
    Vec3 point;
    Vec3 color;
    float height, width, angle;
};
struct Vertex {
    float clip[4];
    Vec3 color;
};
struct Draw {
    GfxRange vertices;
    unsigned count;
};

GfxStageHookHandle stage_hook = 0;
GfxDrawTypeHandle draw_type = 0;
WGPURenderPipeline pipeline = nullptr;
std::uint64_t pipeline_layout = 0;
bool pipeline_reversed = false;
const J3DModelData* cached_model = nullptr;
std::vector<Root> roots;
std::vector<Vertex> vertices;
bool frame_reported = false;

constexpr char Shader[] = R"(
struct Output {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
}
@vertex fn vs_main(@location(0) position: vec4f, @location(1) color: vec3f) -> Output {
    return Output(position, color);
}
@fragment fn fs_main(in: Output) -> @location(0) vec4f {
    return vec4f(in.color, 1.0);
}
)";

Vec3 mix(Vec3 a, Vec3 b, float t) {
    return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}
Vec3 scale(Vec3 a, float factor) {
    return {a.x * factor, a.y * factor, a.z * factor};
}
std::uint16_t be16(const u8* bytes) {
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}
Vec3 rgb565(unsigned color) {
    return {float(color >> 11) / 31, float((color >> 5) & 63) / 63, float(color & 31) / 31};
}

// The qualified original texture is a 256 × 256 repeating CMPR image.
Vec3 grass_color(const u8* image, float u, float v) {
    const auto x = static_cast<unsigned>((u - std::floor(u)) * 256) & 255;
    const auto y = static_cast<unsigned>((v - std::floor(v)) * 256) & 255;
    const auto* block = image + ((y / 8) * 32 + x / 8) * 32 +
                        ((y & 4) ? 16 : 0) + ((x & 4) ? 8 : 0);
    const auto a = be16(block), b = be16(block + 2);
    const unsigned code = (block[4 + y % 4] >> (6 - 2 * (x % 4))) & 3;
    if (code < 2) return rgb565(code == 0 ? a : b);
    if (a <= b) return mix(rgb565(a), rgb565(b), 0.5f);
    return mix(rgb565(a), rgb565(b), float(code - 1) / 3);
}

float carpet_weight(Vec3 color) {
    // Olive grass can have more red than green. Penalize red and blue
    // together so brown and gray ground stays sparse.
    const float green = color.y - 0.65f * color.x - 0.35f * color.z;
    const float t = std::clamp((green + 0.035f) / 0.15f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float random(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return float(state >> 8) * (1.0f / 16777216.0f);
}

J3DModel* find_model() {
    J3DModel* result = nullptr;
    (void)fopAcIt_Executor([](void* raw, void* output) -> int {
        if (!raw || fopAcM_GetName(raw) != fpcNm_BG_e) return 0;
        auto* actor = static_cast<daBg_c*>(raw);
        if (fopAcM_GetRoomNo(actor) != 0 || !actor->mBgParts[0].model) return 0;
        *static_cast<J3DModel**>(output) = actor->mBgParts[0].model;
        return 0;
    }, &result);
    return result;
}

void scatter(J3DModel& instance) {
    auto& model = *instance.getModelData();
    auto* names = model.getMaterialName();
    auto* textures = model.getTexture();
    auto* texture_names = model.getTextureName();
    if (!names || model.getMaterialNum() != 35 || model.getShapeNum() != 35 ||
        std::strcmp(names->getName(12), "aa_MA00_kusa_v") || !textures ||
        !texture_names || !textures->getNum() || std::strcmp(texture_names->getName(0), "I_kusa"))
        return;
    auto* material = model.getMaterialNodePointer(12);
    auto* uvgen = material ? material->getTexCoord(0) : nullptr;
    if (!uvgen || material->getTexNo(0) != 0 || uvgen->getTexGenSrc() != GX_TG_TEX0)
        return;
    const auto* timg = textures->getResTIMG(0);
    const auto* image = textures->getImgDataPtr(0);
    if (timg->format != GX_TF_CMPR || timg->width != 256 || timg->height != 256) return;

    const auto* info = dComIfG_getStageResInfo("R00_00");
    const auto* raw = model.getRawData();
    if (!info || !info->mArchive || !raw) return;
    const auto size = info->mArchive->getExpandedResSize(raw);
    const auto base = reinterpret_cast<std::uintptr_t>(raw);
    auto contains = [&](const void* data, std::size_t bytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);
        return data && size <= 8u * 1024 * 1024 && address >= base &&
               address - base <= size && bytes <= size - (address - base);
    };
    auto& data = model.getVertexData();
    const auto pos_count = data.getVtxArrNum(GX_VA_POS);
    const auto uv_count = data.getVtxArrNum(GX_VA_TEX0);
    if (pos_count > 65536 || uv_count > 65536 || data.getVtxPosType() != GX_S16 ||
        data.getVtxArrStride(GX_VA_POS) != 6 || data.getVtxArrStride(GX_VA_TEX0) != 4 ||
        !contains(data.getVtxPosArray(), pos_count * 6) ||
        !contains(data.getVtxTexCoordArray(0), uv_count * 4) || !contains(image, 32768))
        return;

    std::vector<topology::Format> formats;
    const auto* fmt = data.getVtxAttrFmtList();
    if (!fmt) return;
    bool uv_format = false;
    for (unsigned i = 0; i < 32 && fmt[i].attr != GX_VA_NULL; ++i) {
        formats.push_back({u8(fmt[i].attr), u8(fmt[i].cnt), u8(fmt[i].type)});
        if (fmt[i].attr == GX_VA_TEX0)
            uv_format = fmt[i].cnt == GX_TEX_ST && fmt[i].type == GX_S16 && fmt[i].frac == 9;
    }
    if (!uv_format) return;
    std::vector<Vec3> positions(pos_count);
    const float factor = std::ldexp(1.0f, -int(data.getVtxPosFrac()));
    const auto* packed = static_cast<const std::byte*>(data.getVtxPosArray());
    for (unsigned i = 0; i < pos_count; ++i) {
        std::int16_t p[3];
        std::memcpy(p, packed + i * 6, sizeof(p));
        positions[i] = {p[0] * factor, p[1] * factor, p[2] * factor};
    }
    const auto* uv = static_cast<const std::byte*>(data.getVtxTexCoordArray(0));
    auto read_uv = [&](unsigned i) {
        std::int16_t value[2];
        std::memcpy(value, uv + i * 4, sizeof(value));
        return Vec3{value[0] / 512.0f, value[1] / 512.0f, 0};
    };
    const auto& matrix = instance.getBaseTRMtx();
    std::uint32_t rng = 0x35ab71u;
    unsigned samples = 0;
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        if (!shape || shape->getMaterial() != material || !shape->getVtxDesc()) continue;
        std::vector<topology::Attribute> attributes;
        const auto* desc = shape->getVtxDesc();
        for (unsigned a = 0; a < 32 && desc[a].attr != GX_VA_NULL; ++a)
            attributes.push_back({u8(desc[a].attr), u8(desc[a].type)});
        std::vector<topology::Group> groups;
        for (u16 g = 0; g < shape->getMtxGroupNum(); ++g) {
            auto* draw = shape->getShapeDraw(g);
            if (draw) groups.push_back({draw->getDisplayList(), draw->getDisplayListSize()});
        }
        const topology::Shape input{attributes, groups, 12, true};
        const auto decoded = topology::decode({&input, 1}, formats, positions, data.getNrmNum());
        if (!decoded.ok()) {
            svc_log->warn(mod_ctx, decoded.error);
            continue;
        }
        for (const auto& triangle : decoded.triangles) {
            const auto [a, b, c] = triangle.corners;
            if (std::abs(triangle.face_normal.y) < 0.65f ||
                std::max({a.texcoord, b.texcoord, c.texcoord}) >= uv_count) continue;
            const auto p = positions[a.position], q = positions[b.position], r = positions[c.position];
            const float area = 0.5f * std::abs((q.x - p.x) * (r.z - p.z) - (q.z - p.z) * (r.x - p.x));
            const unsigned count = unsigned(std::min(512.0f, area / 500.0f + random(rng)));
            for (unsigned j = 0; j < count; ++j) {
                float u = random(rng), v = random(rng);
                if (u + v > 1) { u = 1 - u; v = 1 - v; }
                // Keep roots just inside the qualified surface.
                u = 0.025f + u * 0.925f;
                v = 0.025f + v * 0.925f;
                const auto s = mix(p, mix(q, r, v / (u + v)), u + v);
                const auto tex = mix(read_uv(a.texcoord),
                                     mix(read_uv(b.texcoord), read_uv(c.texcoord), v / (u + v)), u + v);
                const Vec3 world{
                    matrix[0][0]*s.x + matrix[0][1]*s.y + matrix[0][2]*s.z + matrix[0][3],
                    matrix[1][0]*s.x + matrix[1][1]*s.y + matrix[1][2]*s.z + matrix[1][3],
                    matrix[2][0]*s.x + matrix[2][1]*s.y + matrix[2][2]*s.z + matrix[2][3]};
                const auto color = grass_color(image, tex.x, tex.y);
                const float carpet = carpet_weight(color);
                if (random(rng) >= std::lerp(0.015f, 1.0f, carpet)) continue;
                const Root root{world, color,
                                std::lerp(15.0f, 5.0f, carpet) + 7 * random(rng),
                                std::lerp(2.0f, 5.0f, carpet) + 2 * random(rng),
                                6.283185f * random(rng)};
                ++samples;
                if (roots.size() < MaxRoots) roots.push_back(root);
                else {
                    const auto slot = unsigned(random(rng) * samples);
                    if (slot < MaxRoots) roots[slot] = root;
                }
            }
        }
    }
    char message[128];
    std::snprintf(message, sizeof(message), "Ground grass: Ordon material 12, %zu roots from %u samples",
                  roots.size(), samples);
    svc_log->info(mod_ctx, message);
}

void encode(ModContext*, const GfxDrawContext* context, const void* payload, size_t size, void*) {
    if (!context || size != sizeof(Draw) ||
        context->layout.depth_stencil_format == WGPUTextureFormat_Undefined) return;
    if (!pipeline || pipeline_layout != context->layout.key || pipeline_reversed != context->uses_reversed_z) {
        if (pipeline) wgpuRenderPipelineRelease(pipeline);
        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
        source.code = {Shader, WGPU_STRLEN};
        WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        shader_desc.nextInChain = &source.chain;
        auto shader = wgpuDeviceCreateShaderModule(context->device, &shader_desc);
        WGPUVertexAttribute attributes[2]{};
        attributes[0].format = WGPUVertexFormat_Float32x4;
        attributes[1].format = WGPUVertexFormat_Float32x3;
        attributes[1].offset = offsetof(Vertex, color);
        attributes[1].shaderLocation = 1;
        WGPUVertexBufferLayout buffer = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        buffer.arrayStride = sizeof(Vertex);
        buffer.stepMode = WGPUVertexStepMode_Vertex;
        buffer.attributeCount = 2;
        buffer.attributes = attributes;
        WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shader;
        fragment.entryPoint = {"fs_main", WGPU_STRLEN};
        fragment.targetCount = gfx_init_color_target_states(&context->layout, colors, nullptr, WGPUColorWriteMask_All);
        fragment.targets = colors;
        WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
        depth.format = context->layout.depth_stencil_format;
        depth.depthWriteEnabled = WGPUOptionalBool_True;
        depth.depthCompare = context->uses_reversed_z ? WGPUCompareFunction_GreaterEqual : WGPUCompareFunction_LessEqual;
        WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX ground grass", WGPU_STRLEN};
        desc.vertex.module = shader;
        desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
        desc.vertex.bufferCount = 1;
        desc.vertex.buffers = &buffer;
        desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        desc.multisample.count = context->layout.sample_count;
        desc.depthStencil = &depth;
        desc.fragment = &fragment;
        pipeline = wgpuDeviceCreateRenderPipeline(context->device, &desc);
        wgpuShaderModuleRelease(shader);
        pipeline_layout = context->layout.key;
        pipeline_reversed = context->uses_reversed_z;
    }
    if (!pipeline) return;
    Draw draw{};
    std::memcpy(&draw, payload, sizeof(draw));
    wgpuRenderPassEncoderSetPipeline(context->pass, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(context->pass, 0, context->vertex_buffer,
                                         draw.vertices.offset, draw.vertices.size);
    wgpuRenderPassEncoderDraw(context->pass, draw.count, 1, 0, 0);
}

void stage(ModContext*, const GfxStageContext* context, void*) {
    const auto* scene = dComIfGp_getStartStageName();
    if (!settings::ground_grass_enabled() || !context || !context->game_view || !scene ||
        std::strcmp(scene, "F_SP103") || dComIfGp_roomControl_getStayNo() != 0) {
        cached_model = nullptr;
        roots.clear();
        return;
    }
    auto* instance = find_model();
    const auto* model = instance ? instance->getModelData() : nullptr;
    if (model != cached_model) {
        cached_model = model;
        roots.clear();
        if (model) {
            scatter(*instance);
            if (roots.empty()) svc_log->warn(mod_ctx, "Ground grass: Ordon surface qualification found no roots");
        }
    }
    if (roots.empty()) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    vertices.clear();
    float closest = DrawDistance * DrawDistance;
    const Vec3 fog{g_env_light.fog_col.r / 255.0f, g_env_light.fog_col.g / 255.0f,
                   g_env_light.fog_col.b / 255.0f};
    auto vertex = [&](Vec3 point, Vec3 color) {
        Vertex result{};
        const auto* m = camera.proj_from_world;
        for (unsigned i = 0; i < 4; ++i)
            result.clip[i] = m[i]*point.x + m[4+i]*point.y + m[8+i]*point.z + m[12+i];
        result.color = color;
        return result;
    };
    for (const auto& root : roots) {
        if (vertices.size() >= MaxVisible * 6) break;
        const float dx = root.point.x - camera.eye[0], dz = root.point.z - camera.eye[2];
        const float dy = root.point.y - camera.eye[1];
        const float distance2 = dx*dx + dy*dy + dz*dz;
        closest = std::min(closest, distance2);
        if (distance2 >= DrawDistance * DrawDistance) continue;
        const float distance = std::sqrt(distance2);
        const float height = root.height * std::clamp((DrawDistance - distance) / 450.0f, 0.0f, 1.0f);
        if (height < 1.0f) continue;
        const auto projected = vertex(root.point, root.color);
        const auto& clip = projected.clip;
        if (clip[3] <= 0 || std::abs(clip[0]) > clip[3] * 1.1f ||
            std::abs(clip[1]) > clip[3] * 1.1f) continue;
        const float haze = std::clamp((distance - g_env_light.mFogNear) /
                            std::max(1.0f, g_env_light.mFogFar - g_env_light.mFogNear), 0.0f, 1.0f);
        const Vec3 base = mix(scale(root.color, 0.88f), fog, haze);
        const Vec3 tip = mix(root.color, fog, haze);
        const float x = std::cos(root.angle) * root.width, z = std::sin(root.angle) * root.width;
        const auto p = root.point;
        for (const auto offset : {Vec3{x, 0, z}, Vec3{-z, 0, x}}) {
            vertices.push_back(vertex({p.x - offset.x, p.y + 0.5f, p.z - offset.z}, base));
            vertices.push_back(vertex({p.x + offset.x, p.y + 0.5f, p.z + offset.z}, base));
            vertices.push_back(vertex({p.x + offset.x * 1.2f, p.y + height, p.z + offset.z * 1.2f}, tip));
        }
    }
    if (!frame_reported) {
        char message[160];
        std::snprintf(message, sizeof(message), "Ground grass: %zu visible roots, nearest %.1f units",
                      vertices.size() / 6, std::sqrt(closest));
        svc_log->info(mod_ctx, message);
        frame_reported = true;
    }
    if (vertices.empty()) return;
    Draw draw{{}, static_cast<unsigned>(vertices.size())};
    if (svc_gfx->push_verts(mod_ctx, vertices.data(), vertices.size() * sizeof(Vertex), 4, &draw.vertices) == MOD_OK)
        (void)svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw));
}
} // namespace

void initialize() {
    if (!svc_gfx || !svc_camera || !svc_camera->get_camera) {
        svc_log->warn(mod_ctx, "Ground grass: graphics or camera service unavailable");
        return;
    }
    GfxDrawTypeDesc draw = GFX_DRAW_TYPE_DESC_INIT;
    draw.label = "MidnaFX ground grass";
    draw.draw = encode;
    if (svc_gfx->register_draw_type(mod_ctx, &draw, &draw_type) != MOD_OK)
        svc_log->warn(mod_ctx, "Ground grass: draw registration failed");
}

void update() {
    if (!draw_type) return;
    const auto* scene = dComIfGp_getStartStageName();
    const bool wanted = settings::ground_grass_enabled() && scene &&
        std::strcmp(scene, "F_SP103") == 0 && dComIfGp_roomControl_getStayNo() == 0;
    if (wanted == (stage_hook != 0)) return;
    if (!wanted) {
        svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
        stage_hook = 0;
        cached_model = nullptr;
        roots.clear();
        return;
    }
    GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
    hook.callback = stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_TERRAIN, &hook, &stage_hook) != MOD_OK)
        shutdown();
    else
        svc_log->info(mod_ctx, "Ground grass: Ordon terrain stage registered");
}

void shutdown() {
    if (stage_hook) svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
    if (draw_type) svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    stage_hook = draw_type = 0;
    if (pipeline) wgpuRenderPipelineRelease(pipeline);
    pipeline = nullptr;
    cached_model = nullptr;
    frame_reported = false;
    std::vector<Root>().swap(roots);
    std::vector<Vertex>().swap(vertices);
}
} // namespace midnafx::render::ground_grass
