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
#include <string_view>
#include <unordered_map>
#include <vector>

namespace midnafx::render::ground_grass {
namespace {
using topology::Vec3;
constexpr unsigned MaxRoots = 720000;
constexpr unsigned MaxVisible = 18000;
constexpr unsigned MaxPebbles = 24000;
constexpr float PebbleDistance = 1800.0f;
constexpr unsigned BladesPerClump = 9;
constexpr unsigned VerticesPerClump = BladesPerClump * 3;
constexpr float DrawDistance = 3400.0f;
constexpr float PathMargin = 18.0f;
constexpr float PathCell = 128.0f;

struct Root {
    Vec3 point;
    Vec3 color;
    float height, width, angle;
};
struct Pebble {
    Root root;
    Vec3 slope;
};
struct Clump {
    Vec3 point;
    Vec3 shape; // Signed height (negative for pebbles), width, and angle.
    Vec3 base;  // Grass base color or pebble surface slope in XY.
    Vec3 tip;
};
struct Draw {
    GfxRange instances;
    GfxRange projection;
    unsigned count;
};

GfxStageHookHandle stage_hook = 0;
GfxDrawTypeHandle draw_type = 0;
WGPURenderPipeline pipeline = nullptr;
std::uint64_t pipeline_layout = 0;
bool pipeline_reversed = false;
const J3DModelData* cached_model = nullptr;
std::vector<Root> roots;
std::vector<Pebble> pebbles;
std::vector<Clump> clumps;
bool frame_reported = false;

constexpr char Shader[] = R"(
struct Output {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
}
@group(0) @binding(0) var<uniform> projection: mat4x4f;
override blades_per_clump: f32;
fn blade_variation(seed: u32) -> vec3f {
    var bits = (seed ^ (seed >> 16u)) * 0x7feb352du;
    bits = (bits ^ (bits >> 15u)) * 0x846ca68bu;
    bits ^= bits >> 16u;
    return vec3f(f32(bits & 255u), f32((bits >> 8u) & 255u),
                 f32((bits >> 16u) & 255u)) / 255.0;
}
fn pebble_vertex(index: u32, root: vec3f, shape: vec3f, slope: vec2f, color: vec3f) -> Output {
    let facet = index / 3u;
    let corner = index % 3u;
    let rim = (facet + corner) % u32(blades_per_clump);
    let variation = blade_variation(bitcast<u32>(shape.z) + rim);
    let angle = shape.z + f32(rim) * (6.2831853 / blades_per_clump);
    var offset = vec2f(cos(angle), sin(angle)) * shape.y * mix(0.72, 1.0, variation.x);
    var rise = -0.35;
    if (corner == 2u) {
        offset = vec2f(cos(shape.z), sin(shape.z)) * shape.y * 0.22;
        rise = -shape.x;
    }
    let light = 0.68 + 0.24 * cos(shape.z + (f32(facet) + 0.5) * (6.2831853 / blades_per_clump));
    let point = root + vec3f(offset.x, rise + dot(slope, offset), offset.y);
    return Output(projection * vec4f(point, 1.0), color * (light + select(0.0, 0.17, corner == 2u)));
}
@vertex fn vs_main(@builtin(vertex_index) index: u32,
                   @location(0) root: vec3f, @location(1) shape: vec3f,
                   @location(2) base: vec3f, @location(3) tip: vec3f) -> Output {
    if (shape.x < 0.0) {
        return pebble_vertex(index, root, shape, base.xy, tip);
    }
    let blade = f32(index / 3u);
    let corner = index % 3u;
    let variation = blade_variation(bitcast<u32>(shape.z) + index / 3u);
    let angle = shape.z + (blade + variation.x - 0.5) * (6.2831853 / blades_per_clump);
    let radial = vec2f(cos(angle), sin(angle));
    let spread = shape.y * mix(0.25, 0.5, variation.y);
    let width = shape.y * 0.10;
    var offset = radial * spread;
    var height = 0.5;
    var color = base;
    if (corner == 2u) {
        offset += radial * spread;
        height = shape.x * mix(0.62, 1.0, variation.z);
        color = tip;
    } else {
        let side = select(-width, width, corner == 0u);
        offset += vec2f(-radial.y, radial.x) * side;
    }
    return Output(projection * vec4f(root + vec3f(offset.x, height, offset.y), 1.0), color);
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

Vec3 surface_color(const u8* image, unsigned width, unsigned height, float u, float v,
                   bool* opaque = nullptr) {
    const auto x = std::min(width - 1, static_cast<unsigned>((u - std::floor(u)) * width));
    const auto y = std::min(height - 1, static_cast<unsigned>((v - std::floor(v)) * height));
    const auto* block = image + ((y / 8) * (width / 8) + x / 8) * 32 +
                        ((y & 4) ? 16 : 0) + ((x & 4) ? 8 : 0);
    const auto a = be16(block), b = be16(block + 2);
    const unsigned code = (block[4 + y % 4] >> (6 - 2 * (x % 4))) & 3;
    if (opaque) *opaque = a > b || code != 3;
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

// Continuous over triangle boundaries and fixed in terrain space.
float scatter_patch(Vec3 point) {
    constexpr float PatchSize = 96.0f;
    const float x = point.x / PatchSize, z = point.z / PatchSize;
    const auto ix = static_cast<int>(std::floor(x)), iz = static_cast<int>(std::floor(z));
    const auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const auto sample = [](int gx, int gz) {
        std::uint32_t bits = std::uint32_t(gx) * 0x9e3779b9u ^ std::uint32_t(gz) * 0x85ebca6bu;
        bits = (bits ^ (bits >> 16)) * 0x7feb352du;
        bits = (bits ^ (bits >> 15)) * 0x846ca68bu;
        return float((bits ^ (bits >> 16)) >> 8) / 16777216.0f;
    };
    const float u = smooth(x - ix), v = smooth(z - iz);
    return std::lerp(std::lerp(sample(ix, iz), sample(ix + 1, iz), u),
                     std::lerp(sample(ix, iz + 1), sample(ix + 1, iz + 1), u), v);
}

int path_cell(float p) {
    return static_cast<int>(std::floor(p / PathCell));
}

std::uint64_t path_key(int x, int z) {
    return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(z);
}

float side(Vec3 a, Vec3 b, Vec3 p) {
    return (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x);
}

struct PathTriangle {
    Vec3 a, b, c;
    float sign, ab, bc, ca;

    bool covers(Vec3 p) const {
        if (p.y < std::min({a.y, b.y, c.y}) - 60 ||
            p.y > std::max({a.y, b.y, c.y}) + 60) return false;
        return sign * side(a, b, p) >= -ab &&
               sign * side(b, c, p) >= -bc &&
               sign * side(c, a, p) >= -ca;
    }
};

J3DModel* find_model() {
    J3DModel* result = nullptr;
    (void)fopAcIt_Executor([](void* raw, void* output) -> int {
        if (!raw || fopAcM_GetName(raw) != fpcNm_BG_e) return 0;
        auto* actor = static_cast<daBg_c*>(raw);
        if (fopAcM_GetRoomNo(actor) != dComIfGp_roomControl_getStayNo() || !actor->mBgParts[0].model)
            return 0;
        *static_cast<J3DModel**>(output) = actor->mBgParts[0].model;
        return 0;
    }, &result);
    return result;
}

void scatter(J3DModel& instance, bool ordon) {
    auto& model = *instance.getModelData();
    auto* names = model.getMaterialName();
    auto* textures = model.getTexture();
    auto* texture_names = model.getTextureName();
    if (!names || !textures || !texture_names) return;
    J3DMaterial* grass = nullptr;
    const u8* grass_image = nullptr;
    if (ordon && model.getMaterialNum() == 35 && model.getShapeNum() == 35 &&
        !std::strcmp(names->getName(12), "aa_MA00_kusa_v") && textures->getNum() &&
        !std::strcmp(texture_names->getName(0), "I_kusa")) {
        auto* material = model.getMaterialNodePointer(12);
        auto* uvgen = material ? material->getTexCoord(0) : nullptr;
        const auto* timg = textures->getResTIMG(0);
        if (uvgen && material->getTexNo(0) == 0 && uvgen->getTexGenSrc() == GX_TG_TEX0 &&
            timg->format == GX_TF_CMPR && timg->width == 256 && timg->height == 256) {
            grass = material;
            grass_image = textures->getImgDataPtr(0);
        }
    }

    char archive[12];
    std::snprintf(archive, sizeof(archive), "R%02d_00", dComIfGp_roomControl_getStayNo());
    const auto* info = dComIfG_getStageResInfo(archive);
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
    const bool float_positions = data.getVtxPosType() == GX_F32;
    const unsigned pos_stride = float_positions ? 12 : 6;
    if (pos_count > 65536 || uv_count > 65536 || (!float_positions && data.getVtxPosType() != GX_S16) ||
        data.getVtxArrStride(GX_VA_POS) != pos_stride || data.getVtxArrStride(GX_VA_TEX0) != 4 ||
        !contains(data.getVtxPosArray(), pos_count * pos_stride) ||
        !contains(data.getVtxTexCoordArray(0), uv_count * 4))
        return;

    std::vector<topology::Format> formats;
    const auto* fmt = data.getVtxAttrFmtList();
    if (!fmt) return;
    int uv_fraction = -1;
    for (unsigned i = 0; i < 32 && fmt[i].attr != GX_VA_NULL; ++i) {
        formats.push_back({u8(fmt[i].attr), u8(fmt[i].cnt), u8(fmt[i].type)});
        if (fmt[i].attr == GX_VA_TEX0)
            if (fmt[i].cnt == GX_TEX_ST && fmt[i].type == GX_S16 && fmt[i].frac <= 15)
                uv_fraction = fmt[i].frac;
    }
    if (uv_fraction < 0) return;
    if (uv_fraction != 9 || !contains(grass_image, 32768)) grass = nullptr;
    std::vector<Vec3> positions(pos_count);
    const float factor = std::ldexp(1.0f, -int(data.getVtxPosFrac()));
    const auto* packed = static_cast<const std::byte*>(data.getVtxPosArray());
    for (unsigned i = 0; i < pos_count; ++i) {
        if (float_positions) {
            float p[3];
            std::memcpy(p, packed + i * pos_stride, sizeof(p));
            positions[i] = {p[0], p[1], p[2]};
            continue;
        }
        std::int16_t p[3];
        std::memcpy(p, packed + i * 6, sizeof(p));
        positions[i] = {p[0] * factor, p[1] * factor, p[2] * factor};
    }
    const auto* uv = static_cast<const std::byte*>(data.getVtxTexCoordArray(0));
    const float uv_scale = std::ldexp(1.0f, -uv_fraction);
    auto read_uv = [&](unsigned i) {
        std::int16_t value[2];
        std::memcpy(value, uv + i * 4, sizeof(value));
        return Vec3{value[0] * uv_scale, value[1] * uv_scale, 0};
    };
    const auto& matrix = instance.getBaseTRMtx();
    auto to_world = [&](Vec3 point) {
        return Vec3{matrix[0][0]*point.x + matrix[0][1]*point.y + matrix[0][2]*point.z + matrix[0][3],
                    matrix[1][0]*point.x + matrix[1][1]*point.y + matrix[1][2]*point.z + matrix[1][3],
                    matrix[2][0]*point.x + matrix[2][1]*point.y + matrix[2][2]*point.z + matrix[2][3]};
    };
    auto decode_shape = [&](J3DShape& shape) {
        std::vector<topology::Attribute> attributes;
        const auto* desc = shape.getVtxDesc();
        for (unsigned a = 0; a < 32 && desc[a].attr != GX_VA_NULL; ++a)
            attributes.push_back({u8(desc[a].attr), u8(desc[a].type)});
        std::vector<topology::Group> groups;
        for (u16 g = 0; g < shape.getMtxGroupNum(); ++g) {
            auto* draw = shape.getShapeDraw(g);
            if (draw) groups.push_back({draw->getDisplayList(), draw->getDisplayListSize()});
        }
        const topology::Shape input{attributes, groups, shape.getMaterial()->getIndex(), true};
        return topology::decode({&input, 1}, formats, positions, data.getNrmNum());
    };

    // The paths are separate translucent meshes laid over the grass terrain.
    // Use their projected footprint before building any blade geometry.
    std::vector<PathTriangle> paths;
    std::unordered_map<std::uint64_t, std::vector<unsigned>> path_cells;
    for (u16 i = 0; grass && i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        if (!shape || !shape->getMaterial() || !shape->getVtxDesc()) continue;
        const auto index = shape->getMaterial()->getIndex();
        if (index != 23 && index != 30 && index != 31) continue;
        const auto decoded = decode_shape(*shape);
        if (!decoded.ok()) {
            svc_log->warn(mod_ctx, decoded.error);
            return;
        }
        for (const auto& triangle : decoded.triangles) {
            const auto a = positions[triangle.corners[0].position];
            const auto b = positions[triangle.corners[1].position];
            const auto c = positions[triangle.corners[2].position];
            const float area = side(a, b, c);
            if (std::abs(area) < 1.0f) continue;
            auto margin = [](Vec3 p, Vec3 q) {
                return PathMargin * std::hypot(p.x - q.x, p.z - q.z);
            };
            const auto id = static_cast<unsigned>(paths.size());
            paths.push_back({a, b, c, area > 0 ? 1.0f : -1.0f,
                             margin(a, b), margin(b, c), margin(c, a)});
            for (int z = path_cell(std::min({a.z, b.z, c.z}) - PathMargin);
                 z <= path_cell(std::max({a.z, b.z, c.z}) + PathMargin); ++z)
                for (int x = path_cell(std::min({a.x, b.x, c.x}) - PathMargin);
                     x <= path_cell(std::max({a.x, b.x, c.x}) + PathMargin); ++x)
                    path_cells[path_key(x, z)].push_back(id);
        }
    }
    if (paths.empty()) grass = nullptr;

    std::uint32_t rng = 0x35ab71u;
    unsigned samples = 0, path_rejected = 0;
    for (u16 i = 0; grass && i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        if (!shape || shape->getMaterial() != grass || !shape->getVtxDesc()) continue;
        const auto decoded = decode_shape(*shape);
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
            const unsigned count = unsigned(std::min(3600.0f, area / 60.0f + random(rng)));
            for (unsigned j = 0; j < count; ++j) {
                float u = random(rng), v = random(rng);
                if (u + v > 1) { u = 1 - u; v = 1 - v; }
                // Keep roots just inside the qualified surface.
                u = 0.025f + u * 0.925f;
                v = 0.025f + v * 0.925f;
                const auto s = mix(p, mix(q, r, v / (u + v)), u + v);
                const auto cell = path_cells.find(path_key(path_cell(s.x), path_cell(s.z)));
                if (cell != path_cells.end() &&
                    std::any_of(cell->second.begin(), cell->second.end(),
                                [&](unsigned id) { return paths[id].covers(s); })) {
                    ++path_rejected;
                    continue;
                }
                const auto tex = mix(read_uv(a.texcoord),
                                     mix(read_uv(b.texcoord), read_uv(c.texcoord), v / (u + v)), u + v);
                const auto world = to_world(s);
                const auto color = surface_color(grass_image, 256, 256, tex.x, tex.y);
                const float carpet = carpet_weight(color);
                const float patch = scatter_patch(s);
                const float coverage = std::lerp(0.015f, 1.0f, std::sqrt(carpet));
                const float patch_density = std::lerp(0.25f, 1.0f, patch);
                if (random(rng) >= coverage * std::lerp(patch_density, 1.0f, carpet)) continue;
                const Root root{world, color,
                                (std::lerp(15.0f, 5.0f, carpet) + 7 * random(rng)) *
                                    std::lerp(0.85f, 1.2f, patch),
                                std::lerp(2.0f, 12.0f, carpet) + 2 * random(rng),
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
    // Match authored ground materials. Overlay dirt gets fewer pebbles than base gravel.
    rng = 0x5f27b6u;
    unsigned pebble_samples = 0;
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        auto* material = shape ? shape->getMaterial() : nullptr;
        if (!material || !shape->getVtxDesc()) continue;
        const std::string_view name = names->getName(material->getIndex());
        const bool path = ordon && (name == "bb_MA00_TuchiMichi_v" || name == "dd_MA00_TuchiMichi_v_x");
        const bool dirt = ordon ? name == "dd_MA00_tuchi_v_x" : name == "aa_MA00_Jari_A_v";
        if (!dirt && !path) continue;
        const auto slot = material->getTexNo(0);
        auto* generator = material->getTexCoord(0);
        if (slot >= textures->getNum() || !generator || generator->getTexGenSrc() != GX_TG_TEX0 ||
            std::strcmp(texture_names->getName(slot), path ? "I_TuchiMichi" : ordon ? "I_hatake2" : "I_tuchi128"))
            continue;
        const auto* texture = textures->getResTIMG(slot);
        const auto* image = textures->getImgDataPtr(slot);
        const unsigned width = texture->width, height = texture->height;
        if (texture->format != GX_TF_CMPR || !width || !height || width % 8 || height % 8 ||
            !contains(image, std::size_t(width) * height / 2)) continue;

        const auto decoded = decode_shape(*shape);
        if (!decoded.ok()) {
            svc_log->warn(mod_ctx, decoded.error);
            continue;
        }
        for (const auto& triangle : decoded.triangles) {
            const auto [a, b, c] = triangle.corners;
            if (std::max({a.texcoord, b.texcoord, c.texcoord}) >= uv_count) continue;
            const auto p = to_world(positions[a.position]), q = to_world(positions[b.position]),
                       r = to_world(positions[c.position]);
            const float area = side(p, q, r);
            if (std::abs(area) < 1.0f) continue;
            const float slope_x = ((q.y - p.y) * (r.z - p.z) - (r.y - p.y) * (q.z - p.z)) / area;
            const float slope_z = ((q.x - p.x) * (r.y - p.y) - (r.x - p.x) * (q.y - p.y)) / area;
            if (slope_x * slope_x + slope_z * slope_z > 0.36f) continue;

            const float sample_area = path ? 3800.0f : 900.0f;
            const auto count = unsigned(std::min(4096.0f, 0.5f * std::abs(area) / sample_area + random(rng)));
            const auto uv_a = read_uv(a.texcoord), uv_b = read_uv(b.texcoord), uv_c = read_uv(c.texcoord);
            for (unsigned j = 0; j < count; ++j) {
                float u = random(rng), v = random(rng);
                if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
                const auto point = mix(p, mix(q, r, v / std::max(u + v, 0.0001f)), u + v);
                if (random(rng) > std::lerp(0.35f, 1.0f, scatter_patch(point))) continue;
                const auto tex = mix(uv_a, mix(uv_b, uv_c, v / std::max(u + v, 0.0001f)), u + v);
                bool opaque;
                auto color = surface_color(image, width, height, tex.x, tex.y, &opaque);
                if (!opaque || carpet_weight(color) > 0.45f) continue;
                color = mix(color, {0.52f, 0.47f, 0.40f}, 0.25f);
                const float size_bias = random(rng);
                const float radius = std::lerp(2.2f, 8.5f, size_bias * size_bias);
                const Pebble pebble{{point, color, radius * std::lerp(0.35f, 0.64f, random(rng)),
                                     radius, 6.283185f * random(rng)}, {slope_x, slope_z, 0.0f}};
                ++pebble_samples;
                if (pebbles.size() < MaxPebbles) pebbles.push_back(pebble);
                else {
                    const auto replacement = unsigned(random(rng) * pebble_samples);
                    if (replacement < MaxPebbles) pebbles[replacement] = pebble;
                }
            }
        }
    }
    char message[128];
    std::snprintf(message, sizeof(message), "Ground cover: %zu grass roots, %zu pebbles, %zu path triangles, %u path rejections",
                  roots.size(), pebbles.size(), paths.size(), path_rejected);
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
        const size_t offsets[] = {offsetof(Clump, point), offsetof(Clump, shape),
                                  offsetof(Clump, base), offsetof(Clump, tip)};
        WGPUVertexAttribute attributes[4]{};
        for (unsigned i = 0; i < 4; ++i) {
            attributes[i].format = WGPUVertexFormat_Float32x3;
            attributes[i].offset = offsets[i];
            attributes[i].shaderLocation = i;
        }
        WGPUVertexBufferLayout buffer = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        buffer.arrayStride = sizeof(Clump);
        buffer.stepMode = WGPUVertexStepMode_Instance;
        buffer.attributeCount = 4;
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
        WGPUConstantEntry blade_count = WGPU_CONSTANT_ENTRY_INIT;
        blade_count.key = {"blades_per_clump", WGPU_STRLEN};
        blade_count.value = BladesPerClump;
        desc.vertex.constantCount = 1;
        desc.vertex.constants = &blade_count;
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
    auto layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.buffer = context->uniform_buffer;
    entry.offset = draw.projection.offset;
    entry.size = draw.projection.size;
    WGPUBindGroupDescriptor binding = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    binding.layout = layout;
    binding.entryCount = 1;
    binding.entries = &entry;
    auto group = wgpuDeviceCreateBindGroup(context->device, &binding);
    wgpuBindGroupLayoutRelease(layout);
    if (!group) return;
    wgpuRenderPassEncoderSetPipeline(context->pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(context->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(context->pass, 0, context->vertex_buffer,
                                         draw.instances.offset, draw.instances.size);
    wgpuRenderPassEncoderDraw(context->pass, VerticesPerClump, draw.count, 0, 0);
    wgpuBindGroupRelease(group);
}

bool supported_scene(const char* scene) {
    if (!scene) return false;
    const int room = dComIfGp_roomControl_getStayNo();
    return (room == 0 && !std::strcmp(scene, "F_SP103")) ||
           ((room == 7 || room == 9) && !std::strcmp(scene, "F_SP121"));
}

void stage(ModContext*, const GfxStageContext* context, void*) {
    const auto* scene = dComIfGp_getStartStageName();
    if (!settings::ground_grass_enabled() || !context || !context->game_view || !supported_scene(scene)) {
        cached_model = nullptr;
        roots.clear();
        pebbles.clear();
        return;
    }
    auto* instance = find_model();
    const auto* model = instance ? instance->getModelData() : nullptr;
    if (model != cached_model) {
        cached_model = model;
        roots.clear();
        pebbles.clear();
        if (model) {
            frame_reported = false;
            scatter(*instance, !std::strcmp(scene, "F_SP103"));
            if (roots.empty() && pebbles.empty())
                svc_log->warn(mod_ctx, "Ground cover: surface qualification found no instances");
        }
    }
    if (roots.empty() && pebbles.empty()) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    clumps.clear();
    float closest = DrawDistance * DrawDistance;
    const Vec3 fog{g_env_light.fog_col.r / 255.0f, g_env_light.fog_col.g / 255.0f,
                   g_env_light.fog_col.b / 255.0f};
    const auto* m = camera.proj_from_world;
    auto visible = [&](Vec3 point) {
        float clip[4];
        for (unsigned i = 0; i < 4; ++i)
            clip[i] = m[i]*point.x + m[4+i]*point.y + m[8+i]*point.z + m[12+i];
        return clip[3] > 0 && std::abs(clip[0]) <= clip[3] * 1.1f &&
               std::abs(clip[1]) <= clip[3] * 1.1f;
    };
    auto fog_weight = [&](float distance) {
        return std::clamp((distance - g_env_light.mFogNear) /
                          std::max(1.0f, g_env_light.mFogFar - g_env_light.mFogNear), 0.0f, 1.0f);
    };
    for (const auto& root : roots) {
        if (clumps.size() >= MaxVisible) break;
        const float dx = root.point.x - camera.eye[0], dz = root.point.z - camera.eye[2];
        const float dy = root.point.y - camera.eye[1];
        const float distance2 = dx*dx + dy*dy + dz*dz;
        closest = std::min(closest, distance2);
        if (distance2 >= DrawDistance * DrawDistance) continue;
        const float distance = std::sqrt(distance2);
        const float height = root.height * 1.25f * std::clamp((DrawDistance - distance) / 450.0f, 0.0f, 1.0f);
        if (height < 1.0f) continue;
        if (!visible(root.point)) continue;
        const float haze = fog_weight(distance);
        const Vec3 base = mix(scale(root.color, 0.88f), fog, haze);
        const Vec3 tip = mix(root.color, fog, haze);
        clumps.push_back({root.point, {height, root.width, root.angle}, base, tip});
    }
    const auto grass_count = clumps.size();
    const auto ambient = g_env_light.bg_amb_col[0];
    const auto terrain_light = [](float channel) {
        return std::clamp(0.50f + 0.50f * channel / 255.0f, 0.50f, 1.0f);
    };
    const Vec3 light{terrain_light(ambient.r), terrain_light(ambient.g), terrain_light(ambient.b)};
    const bool twilight = dKy_darkworld_check();
    for (const auto& pebble : pebbles) {
        const auto& root = pebble.root;
        const float dx = root.point.x - camera.eye[0], dy = root.point.y - camera.eye[1],
                    dz = root.point.z - camera.eye[2];
        const float distance2 = dx*dx + dy*dy + dz*dz;
        if (distance2 >= PebbleDistance * PebbleDistance || !visible(root.point)) continue;
        const float distance = std::sqrt(distance2);
        const float fade = std::clamp((PebbleDistance - distance) / 350.0f, 0.0f, 1.0f);
        const float luminance = 0.21f * root.color.x + 0.72f * root.color.y + 0.07f * root.color.z;
        const Vec3 color = twilight ? scale({0.58f, 0.84f, 1.08f}, luminance) : root.color;
        const Vec3 lit{color.x * light.x, color.y * light.y, color.z * light.z};
        clumps.push_back({root.point, {-root.height * fade, root.width * fade, root.angle},
                          pebble.slope, mix(lit, fog, fog_weight(distance))});
    }
    if (!frame_reported && !clumps.empty()) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Ground cover: %zu visible grass roots, %zu visible pebbles, nearest grass %.1f units",
                      grass_count, clumps.size() - grass_count, std::sqrt(closest));
        svc_log->info(mod_ctx, message);
        frame_reported = true;
    }
    if (clumps.empty()) return;
    Draw draw{{}, {}, static_cast<unsigned>(clumps.size())};
    if (svc_gfx->push_uniform(mod_ctx, camera.proj_from_world, sizeof(camera.proj_from_world), &draw.projection) == MOD_OK &&
        svc_gfx->push_verts(mod_ctx, clumps.data(), clumps.size() * sizeof(Clump), 4, &draw.instances) == MOD_OK)
        (void)svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw));
}
} // namespace

void initialize() {
    if (!svc_gfx || !svc_gfx->push_uniform || !svc_camera || !svc_camera->get_camera) {
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
    const bool wanted = settings::ground_grass_enabled() && supported_scene(scene);
    if (wanted == (stage_hook != 0)) return;
    if (!wanted) {
        svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
        stage_hook = 0;
        cached_model = nullptr;
        roots.clear();
        pebbles.clear();
        return;
    }
    GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
    hook.callback = stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_TERRAIN, &hook, &stage_hook) != MOD_OK)
        shutdown();
    else
        svc_log->info(mod_ctx, "Ground cover: terrain stage registered");
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
    std::vector<Pebble>().swap(pebbles);
    std::vector<Clump>().swap(clumps);
}
} // namespace midnafx::render::ground_grass
