#include "ground_grass.hpp"
#include "waterfall.hpp"
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
#include <d/actor/d_a_bg_obj.h>
#include <d/actor/d_a_set_bgobj.h>
#include <d/d_com_inf_game.h>
#include <d/d_resorce.h>
#include <f_op/f_op_actor_iter.h>
#include <f_op/f_op_actor_mng.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <string>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace midnafx::render::ground_grass {
namespace {
using topology::Vec3;
constexpr unsigned MaxRoots = 1100000;
constexpr unsigned MaxVisible = 28000;
constexpr unsigned MaxPebbles = 24000;
constexpr float PebbleDistance = 1800.0f;
constexpr unsigned BladesPerClump = 9;
constexpr unsigned VerticesPerClump = BladesPerClump * 3;
constexpr float DrawDistance = 3400.0f;
constexpr float PathMargin = 18.0f;
constexpr float PathCell = 128.0f;
constexpr float GrassTileSize = 512.0f;

struct Root {
    Vec3 point;
    Vec3 color;
    float height, width, angle;
};
enum class DetailStyle { Stone, Leaf, Twig, Plant, Moss, Count };
struct Pebble {
    Root root;
    Vec3 slope; // XY hold ground slope, Z holds DetailStyle.
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
GfxStageHookHandle waterfall_hook = 0;
GfxStageHookHandle waterfall_contact_hook = 0;
CameraInfo waterfall_camera = CAMERA_INFO_INIT;
waterfall::FrameClip waterfall_clip{};
bool waterfall_contact_ready = false;
GfxDrawTypeHandle draw_type = 0;
WGPURenderPipeline pipeline = nullptr;
std::uint64_t pipeline_layout = 0;
bool pipeline_reversed = false;
struct GrassTile {
    float x, z;
    std::vector<Root> roots;
};
struct RoomCover {
    const J3DModel* instance = nullptr;
    const J3DModelData* model = nullptr;
    std::vector<GrassTile> grass_tiles;
    std::vector<Pebble> pebbles;
    std::vector<waterfall::Fall> falls;
};
constexpr unsigned RoomSlots = 64;
constexpr unsigned PartsPerRoom = 6;
std::array<std::array<RoomCover, PartsPerRoom>, RoomSlots> room_cover;
std::string cached_stage;
struct ObjectFall {
    RoomCover cover;
    float transform[12]{};
    bool present = false;
};
std::unordered_map<const J3DModel*, ObjectFall> object_falls;
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
fn litter_vertex(index: u32, root: vec3f, shape: vec3f, surface: vec3f, color: vec3f) -> Output {
    let facet = index / 3u;
    let corner = index % 3u;
    let style = u32(surface.z);
    let seed = bitcast<u32>(shape.z);
    let variation = blade_variation(seed);
    var point = vec2f(0.0);
    var height = 0.5;
    var shade = 0.82;
    var angle = shape.z;
    if (style == 1u) {
        // Eight rim segments and a lifted midrib form a curled leaf.
        let rim = (facet + corner) % 8u;
        let theta = f32(rim) * 0.78539816;
        if (facet < 8u && corner < 2u) {
            point = vec2f(cos(theta), sin(theta) * mix(0.32, 0.65, variation.x));
            height += -shape.x * (0.20 + 0.55 * max(0.0, point.x));
            shade = 0.77 + 0.20 * sin(theta);
        } else {
            height = -shape.x;
            shade = 1.06;
        }
    } else if (style == 2u) {
        if (facet == 8u) {
            return Output(projection * vec4f(root, 1.0), color);
        }
        // A tapered bent stem and a short offshoot, each made from a strip.
        let branch = facet >= 6u;
        let segment = facet / 2u;
        let end = select(corner == 2u, corner != 0u, facet % 2u == 1u);
        let right = select(corner == 1u, corner != 1u, facet % 2u == 1u);
        var along = min(1.0, f32(segment + u32(end)) / 3.0);
        var center = vec2f(2.0 * along - 1.0, sin(along * 3.141593) * 0.21);
        if (branch) {
            along = f32(end);
            center = vec2f(-0.34 + along * 0.68, 0.18 + along * 0.47);
        }
        let half_width = mix(select(0.095, 0.062, branch), 0.017, along);
        point = center + vec2f(0.0, select(-half_width, half_width, right));
        height += -shape.x * mix(0.85, 0.40, along);
        shade = select(0.70, 1.02, right);
    } else if (style == 3u) {
        // Three raised leaflets share a root. Alternate fold shading per facet.
        let leaflet = facet / 3u;
        let face = facet % 3u;
        let verts = array<vec3f, 5>(vec3f(0.0, 0.0, 0.10), vec3f(0.48, -0.28, 0.31),
                                   vec3f(0.50, 0.0, 0.525), vec3f(0.52, 0.27, 0.34),
                                   vec3f(1.0, 0.0, 0.95));
        let faces = array<vec3u, 3>(vec3u(0, 1, 2), vec3u(1, 4, 2), vec3u(0, 4, 3));
        let vert = verts[faces[face][corner]];
        point = vert.xy * mix(0.64, 1.0, blade_variation(seed + leaflet).x);
        height += -shape.x * vert.z;
        angle += f32(leaflet) * 2.0943951;
        shade = select(0.76, 1.08, face == 2u);
    } else {
        // Overlapping low moss lobes hug the parent slope.
        let lobe = facet / 3u;
        let rim = (facet % 3u + corner) % 3u;
        let theta = f32(rim) * 2.0943951;
        let jitter = blade_variation(seed + lobe);
        point = vec2f(0.36, 0.0);
        if (corner < 2u) {
            point += vec2f(cos(theta), sin(theta)) * mix(0.47, 0.72, jitter.x);
            shade = mix(0.62, 0.90, jitter.y);
        } else {
            height += -shape.x * mix(0.48, 1.0, jitter.z);
            shade = 1.05;
        }
        angle += f32(lobe) * 2.0943951;
    }
    let axis = vec2f(cos(angle), sin(angle));
    let offset = shape.y * vec2f(point.x * axis.x - point.y * axis.y, point.x * axis.y + point.y * axis.x);
    let position = root + vec3f(offset.x, height + dot(surface.xy, offset), offset.y);
    return Output(projection * vec4f(position, 1.0), color * shade);
}
@vertex fn vs_main(@builtin(vertex_index) index: u32,
                   @location(0) root: vec3f, @location(1) shape: vec3f,
                   @location(2) base: vec3f, @location(3) tip: vec3f) -> Output {
    if (shape.x < 0.0) {
        if (base.z > 0.5) {
            return litter_vertex(index, root, shape, base, tip);
        }
        return pebble_vertex(index, root, shape, base.xy, tip);
    }
    let blade = f32(index / 3u);
    let corner = index % 3u;
    let variation = blade_variation(bitcast<u32>(shape.z) + index / 3u);
    let angle = shape.z + (blade + variation.x - 0.5) * (6.2831853 / blades_per_clump);
    let radial = vec2f(cos(angle), sin(angle));
    let spread = min(8.5, shape.y * mix(0.32, 0.66, variation.y));
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

enum class Cover { None, Grass, Gravel, Path, Litter, Moss };

std::string lowercase(std::string_view name) {
    std::string result(name);
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

bool has_any(std::string_view name, std::initializer_list<std::string_view> words) {
    return std::any_of(words.begin(), words.end(), [&](auto word) { return name.find(word) != name.npos; });
}

Cover classify(std::string_view material, std::string_view texture) {
    if (has_any(material, {"water", "mizusoko", "river", "kabe", "wall", "roof", "yane", "seal",
                           "shadow", "fog", "nuki", "kusanuki", "saku", "miki", "rockwall"}) &&
        !has_any(material, {"michi", "tuchimichi"}))
        return Cover::None;
    if (has_any(material, {"tuchimichi", "tutimiti", "sunamichi", "road_tsuchi", "tuchi_michi"}))
        return Cover::Path;
    if (has_any(texture, {"groundmoriwood", "tuchiwood", "c_a_hm_tuti", "c_hm_tuti", "ochiba"}) ||
        (texture == "c_woodsita" && has_any(material, {"kiwatuti", "ground", "jimen"})))
        return Cover::Litter;
    if (material.starts_with("aa_") &&
        (has_any(material, {"iwakusa", "iwa_kusa", "kusaiwa", "moss", "kokeground", "aa_koke"}) ||
         (material == "aa_siba_v" && texture == "kkoke02")) &&
        !has_any(material, {"wood", "kikoke", "pole", "kokeita"}) &&
        has_any(texture, {"kusa", "moss", "koke"}))
        return Cover::Moss;
    if (material.starts_with("aa_") && !has_any(material, {"blend", "kusaset", "kiwa"}) &&
        has_any(material, {"kusa", "grass", "siba", "shiba"}) &&
        has_any(texture, {"kusa", "grass", "siba", "shiba"}) &&
        !has_any(texture, {"wall", "kabe", "nuki", "kusari", "kusaset"}))
        return Cover::Grass;
    if ((material.starts_with("aa_") &&
         has_any(material, {"jari", "gravel", "rock_ground", "rockground", "tuchi_v", "tuti_v", "tsuchi_v",
                            "tuchi_a_v", "tuti00_v", "tuti01_v", "tuchi128"})) ||
        material == "dd_ma00_tuchi_v_x")
        return Cover::Gravel;
    return Cover::None;
}

void scatter(J3DModel& instance, int room, unsigned part, bool ordon, bool animated, RoomCover& cover,
             const dRes_info_c* object_archive = nullptr) {
    std::vector<Root> roots;
    auto& pebbles = cover.pebbles;
    const unsigned root_limit = ordon ? MaxRoots : part == 0 ? MaxRoots / 4 : MaxRoots / 16;
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
    std::snprintf(archive, sizeof(archive), "R%02d_00", room);
    const auto* info = object_archive ? object_archive : dComIfG_getStageResInfo(archive);
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
        data.getVtxArrStride(GX_VA_POS) != pos_stride ||
        !contains(data.getVtxPosArray(), pos_count * pos_stride))
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
    // The native array count can include an alignment record at the end.
    const auto valid_position = [](Vec3 p) {
        const auto valid = [](float v) { return std::isfinite(v) && std::abs(v) <= 1.0e7f; };
        return valid(p.x) && valid(p.y) && valid(p.z);
    };
    while (!positions.empty() && !valid_position(positions.back())) positions.pop_back();
    if (positions.empty() || !std::all_of(positions.begin(), positions.end(), valid_position)) return;
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

    if (settings::enhanced_water_enabled()) {
        std::vector<waterfall::Triangle> basin;
        if (object_archive) {
            for (u16 i = 0; i < model.getShapeNum(); ++i) {
                auto* shape = model.getShapeNodePointer(i);
                auto* material = shape ? shape->getMaterial() : nullptr;
                if (!material || !shape->getVtxDesc() ||
                    std::strcmp(names->getName(material->getIndex()), "cc_MA06_NigoriWater_v_x")) continue;
                const auto decoded = decode_shape(*shape);
                if (!decoded.ok()) continue;
                for (const auto& tri : decoded.triangles) {
                    if (std::abs(tri.face_normal.y) < 0.99f) continue;
                    const auto& c = tri.corners;
                    basin.push_back({to_world(positions[c[0].position]), to_world(positions[c[1].position]),
                                     to_world(positions[c[2].position])});
                }
            }
        }
        for (u16 i = 0; i < model.getShapeNum(); ++i) {
            auto* shape = model.getShapeNodePointer(i);
            auto* material = shape ? shape->getMaterial() : nullptr;
            if (!material || !shape->getVtxDesc()) continue;
            const auto name = lowercase(names->getName(material->getIndex()));
            const bool faron = cached_stage == "F_SP108" && room == 1 && part == 0 &&
                               (name == "cf_ma03_water02_v" || name == "cf_ma03_water02_v_x");
            const bool cave = object_archive && cached_stage == "F_SP115" && name == "ce_ma03_waterkasan_v_x";
            if (!faron && !cave && !has_any(name, {"_taki_", "_takikasan_", "_waterfall_", "_cascade_"})) continue;
            const auto decoded = decode_shape(*shape);
            if (!decoded.ok()) {
                if (cave) svc_log->warn(mod_ctx, decoded.error);
                continue;
            }
            std::vector<waterfall::Triangle> sheet;
            for (const auto& tri : decoded.triangles) {
                if (std::abs(tri.face_normal.y) >= (cave ? 0.94f : 0.83f)) continue;
                const auto& c = tri.corners;
                sheet.push_back({to_world(positions[c[0].position]), to_world(positions[c[1].position]),
                                 to_world(positions[c[2].position])});
            }
            const auto count = cover.falls.size();
            waterfall::collect(sheet, cover.falls, cave ? std::span<const waterfall::Triangle>(basin) :
                                                        std::span<const waterfall::Triangle>{}, cave);
            for (size_t j = count; j < cover.falls.size(); ++j) {
                const auto& fall = cover.falls[j];
                char message[200];
                std::snprintf(message, sizeof(message),
                              "Waterfall: %s room %d part %u %s, %zu faces, %.0f x %.0f, foot %.0f %.0f %.0f",
                              cached_stage.c_str(), room, part, name.c_str(), fall.sheet.size(), fall.width, fall.height,
                              fall.foot.x, fall.foot.y, fall.foot.z);
                svc_log->info(mod_ctx, message);
            }
        }
    }
    if (!settings::ground_grass_enabled() || animated || uv_fraction < 0 ||
        data.getVtxArrStride(GX_VA_TEX0) != 4 || !contains(uv, uv_count * 4)) return;

    struct Surface {
        Cover kind = Cover::None;
        const u8* image = nullptr;
        unsigned width = 0, height = 0;
    };
    std::vector<Surface> surfaces(model.getShapeNum());
    const auto* stage_info = dComIfG_getStageResInfo("Stg_00");
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        auto* material = shape ? shape->getMaterial() : nullptr;
        if (!material || !shape->getVtxDesc()) continue;
        const auto slot = material->getTexNo(0);
        if (slot >= textures->getNum()) continue;
        const auto name = lowercase(names->getName(material->getIndex()));
        const auto* texture_name = texture_names->getName(slot);
        auto& surface = surfaces[i];
        surface.kind = classify(name, lowercase(texture_name));
        if (ordon && material == grass) surface.kind = Cover::Grass;
        if (surface.kind == Cover::None) continue;
        auto* generator = material->getTexCoord(0);
        if (!generator || generator->getTexGenSrc() != GX_TG_TEX0) continue;
        const auto* texture = textures->getResTIMG(slot);
        const auto* image = textures->getImgDataPtr(slot);
        const unsigned width = texture->width, height = texture->height;
        if (texture->format != GX_TF_CMPR || !width || !height || width % 8 || height % 8) continue;
        const auto bytes = std::size_t(width) * height / 2;
        if (!contains(image, bytes)) {
            if (!stage_info || !stage_info->mArchive || std::strlen(texture_name) > 58) continue;
            char resource[64];
            std::snprintf(resource, sizeof(resource), "%s.bti", texture_name);
            const auto* bti = static_cast<const ResTIMG*>(dComIfG_getStageRes(resource));
            if (!bti) continue;
            const auto length = stage_info->mArchive->getExpandedResSize(bti);
            if (length > 8u * 1024 * 1024 || length < sizeof(ResTIMG) ||
                bti->imageOffset < sizeof(ResTIMG) || bti->imageOffset > length ||
                bytes > length - bti->imageOffset ||
                reinterpret_cast<const u8*>(bti) + bti->imageOffset != image) continue;
        }
        surface.image = image;
        surface.width = width;
        surface.height = height;
    }

    // The paths are separate translucent meshes laid over the grass terrain.
    // Use their projected footprint before building any blade geometry.
    std::vector<PathTriangle> paths;
    std::unordered_map<std::uint64_t, std::vector<unsigned>> path_cells;
    std::vector<unsigned> wide_paths;
    const bool needs_mask = std::any_of(surfaces.begin(), surfaces.end(), [](const auto& surface) {
        return surface.image && (surface.kind == Cover::Grass || surface.kind == Cover::Litter ||
                                 surface.kind == Cover::Moss);
    });
    for (u16 i = 0; needs_mask && i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        if (!shape || !shape->getMaterial() || !shape->getVtxDesc()) continue;
        const auto index = shape->getMaterial()->getIndex();
        if (surfaces[i].kind != Cover::Path &&
            !(ordon && (index == 23 || index == 30 || index == 31))) continue;
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
            const int min_z = path_cell(std::min({a.z, b.z, c.z}) - PathMargin);
            const int max_z = path_cell(std::max({a.z, b.z, c.z}) + PathMargin);
            const int min_x = path_cell(std::min({a.x, b.x, c.x}) - PathMargin);
            const int max_x = path_cell(std::max({a.x, b.x, c.x}) + PathMargin);
            if (std::int64_t(max_z - min_z + 1) * (max_x - min_x + 1) > 4096) {
                wide_paths.push_back(id);
                continue;
            }
            for (int z = min_z; z <= max_z; ++z)
                for (int x = min_x; x <= max_x; ++x)
                    path_cells[path_key(x, z)].push_back(id);
        }
    }
    auto on_path = [&](Vec3 point) {
        const auto covers = [&](unsigned id) { return paths[id].covers(point); };
        const auto cell = path_cells.find(path_key(path_cell(point.x), path_cell(point.z)));
        return (cell != path_cells.end() && std::any_of(cell->second.begin(), cell->second.end(), covers)) ||
               std::any_of(wide_paths.begin(), wide_paths.end(), covers);
    };

    std::uint32_t rng = 0x35ab71u;
    unsigned samples = 0, path_rejected = 0;
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        const auto& surface = surfaces[i];
        if (surface.kind != Cover::Grass || !surface.image) continue;
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
            const unsigned count = unsigned(std::min(5200.0f, area / (ordon ? 32.0f : 52.0f) + random(rng)));
            for (unsigned j = 0; j < count; ++j) {
                float u = random(rng), v = random(rng);
                if (u + v > 1) { u = 1 - u; v = 1 - v; }
                // Keep roots just inside the qualified surface.
                u = 0.002f + u * 0.994f;
                v = 0.002f + v * 0.994f;
                const auto s = mix(p, mix(q, r, v / (u + v)), u + v);
                if (on_path(s)) {
                    ++path_rejected;
                    continue;
                }
                const auto tex = mix(read_uv(a.texcoord),
                                     mix(read_uv(b.texcoord), read_uv(c.texcoord), v / (u + v)), u + v);
                const auto world = to_world(s);
                bool opaque;
                const auto color = surface_color(surface.image, surface.width, surface.height, tex.x, tex.y, &opaque);
                if (!ordon && !opaque) continue;
                const float carpet = carpet_weight(color);
                const float patch = scatter_patch(s);
                const float coverage = std::lerp(0.07f, 1.0f, std::sqrt(carpet));
                const float patch_density = std::lerp(0.62f, 1.0f, patch);
                if (random(rng) >= coverage * std::lerp(patch_density, 1.0f, carpet)) continue;
                const Root root{world, color,
                                (std::lerp(15.0f, 5.0f, carpet) + 7 * random(rng)) *
                                    std::lerp(0.85f, 1.2f, patch),
                                std::lerp(2.4f, 13.2f, carpet) + 2 * random(rng),
                                6.283185f * random(rng)};
                ++samples;
                if (roots.size() < root_limit) roots.push_back(root);
                else {
                    const auto slot = unsigned(random(rng) * samples);
                    if (slot < root_limit) roots[slot] = root;
                }
            }
        }
    }
    // Keep distant tiles out of the per-frame root scan.
    std::unordered_map<std::uint64_t, size_t> tile_indices;
    for (const auto& root : roots) {
        const int x = static_cast<int>(std::floor(root.point.x / GrassTileSize));
        const int z = static_cast<int>(std::floor(root.point.z / GrassTileSize));
        const auto [tile, inserted] = tile_indices.try_emplace(path_key(x, z), cover.grass_tiles.size());
        if (inserted) cover.grass_tiles.push_back({x * GrassTileSize, z * GrassTileSize, {}});
        cover.grass_tiles[tile->second].roots.push_back(root);
    }
    // Match authored ground materials. Overlay dirt gets fewer pebbles than base gravel.
    rng = 0x5f27b6u;
    unsigned pebble_samples = 0;
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        const auto& surface = surfaces[i];
        const bool path = surface.kind == Cover::Path;
        const bool litter = surface.kind == Cover::Litter;
        const bool moss = surface.kind == Cover::Moss;
        if (!surface.image || (!path && !litter && !moss && surface.kind != Cover::Gravel)) continue;

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
            if (slope_x * slope_x + slope_z * slope_z > (moss ? 0.81f : 0.36f)) continue;

            const float sample_area = path ? 3800.0f : litter ? 1150.0f : moss ? 850.0f : 900.0f;
            const auto count = unsigned(std::min(4096.0f, 0.5f * std::abs(area) / sample_area + random(rng)));
            const auto uv_a = read_uv(a.texcoord), uv_b = read_uv(b.texcoord), uv_c = read_uv(c.texcoord);
            for (unsigned j = 0; j < count; ++j) {
                float u = random(rng), v = random(rng);
                if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
                const auto point = mix(p, mix(q, r, v / std::max(u + v, 0.0001f)), u + v);
                const float patch = scatter_patch(point);
                if (random(rng) > std::lerp(litter || moss ? 0.12f : 0.35f, 1.0f, patch)) continue;
                const auto tex = mix(uv_a, mix(uv_b, uv_c, v / std::max(u + v, 0.0001f)), u + v);
                bool opaque;
                auto color = surface_color(surface.image, surface.width, surface.height, tex.x, tex.y, &opaque);
                if (!opaque) continue;
                const float green = carpet_weight(color);
                if ((moss && green < 0.38f) || (!moss && !litter && green > 0.45f)) continue;
                if (litter || moss) {
                    const auto local = mix(positions[a.position], mix(positions[b.position], positions[c.position],
                                           v / std::max(u + v, 0.0001f)), u + v);
                    if (on_path(local)) continue;
                }
                auto style = DetailStyle::Stone;
                const float size_bias = random(rng);
                const float radius = std::lerp(2.2f, 8.5f, size_bias * size_bias);
                float height = radius * std::lerp(0.35f, 0.64f, random(rng));
                float width = radius;
                if (litter) {
                    const float variant = random(rng);
                    if (green > 0.18f && patch > 0.42f && variant < 0.18f) {
                        style = DetailStyle::Plant;
                        height = radius * 2.15f;
                        width = radius * 2.40f;
                        color = mix(color, {0.31f, 0.45f, 0.18f}, 0.58f);
                    } else if (variant < 0.29f) {
                        style = DetailStyle::Twig;
                        height = 1.35f;
                        width = radius * 2.55f;
                        color = mix(color, {0.32f, 0.22f, 0.12f}, 0.60f);
                    } else {
                        style = DetailStyle::Leaf;
                        height = radius * 0.36f;
                        width = radius * 1.8f;
                        const Vec3 dry = mix({0.38f, 0.20f, 0.09f}, {0.67f, 0.44f, 0.16f}, variant);
                        color = mix(color, dry, 0.56f);
                    }
                } else if (moss) {
                    style = DetailStyle::Moss;
                    height = radius * 0.28f;
                    width = radius * 3.2f;
                    color = mix(color, {0.34f, 0.44f, 0.18f}, 0.32f);
                } else {
                    color = mix(color, {0.52f, 0.47f, 0.40f}, 0.25f);
                }
                const Pebble pebble{{point, color, height, width, 6.283185f * random(rng)},
                                    {slope_x, slope_z, static_cast<float>(style)}};
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
    std::snprintf(message, sizeof(message), "Ground cover room %d part %u: %zu grass roots, %zu stones/litter, %zu path triangles, %u path rejections",
                  room, part, roots.size(), pebbles.size(), paths.size(), path_rejected);
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
        desc.label = {"MidnaFX ground cover", WGPU_STRLEN};
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

void clear_cover() {
    for (auto& room : room_cover) room = {};
    object_falls.clear();
    cached_stage.clear();
}

void update_object_falls() {
    if (cached_stage != "F_SP115" || !settings::enhanced_water_enabled()) return;
    for (auto& [model, entry] : object_falls) entry.present = false;
    (void)fopAcIt_Executor([](void* raw, void*) -> int {
        if (!raw || fopAcM_GetName(raw) != fpcNm_BG_OBJ_e) return 0;
        auto* actor = static_cast<daBgObj_c*>(raw);
        if (actor->field_0xcc8 >= 2) return 0;
        const int room = fopAcM_GetRoomNo(actor);
        for (auto* instance : actor->field_0x5a8[actor->field_0xcc8]) {
            auto* model = instance ? instance->getModelData() : nullptr;
            if (!model) continue;
            auto& entry = object_falls[instance];
            entry.present = true;
            const auto matrix = instance->getBaseTRMtx();
            if (entry.cover.model == model && !std::memcmp(entry.transform, matrix, sizeof(entry.transform)))
                continue;
            entry.cover = {};
            entry.cover.model = model;
            std::memcpy(entry.transform, matrix, sizeof(entry.transform));
            const auto* names = model->getMaterialName();
            if (!names) continue;
            bool cave = false;
            for (u16 i = 0; i < model->getMaterialNum(); ++i)
                if (!std::strcmp(names->getName(i), "ce_MA03_WaterKasan_v_x")) { cave = true; break; }
            if (!cave) continue;
            const auto* archive = dComIfG_getObjectResInfo(daSetBgObj_c::getArcName(actor));
            if (archive) scatter(*instance, room, 0, false, true, entry.cover, archive);
        }
        return 0;
    }, nullptr);
    std::erase_if(object_falls, [](const auto& entry) { return !entry.second.present; });
}

std::vector<const waterfall::Fall*> loaded_falls() {
    std::vector<const waterfall::Fall*> falls;
    for (const auto& room : room_cover)
        for (const auto& part : room)
            for (const auto& fall : part.falls) falls.push_back(&fall);
    for (const auto& [model, entry] : object_falls)
        for (const auto& fall : entry.cover.falls) falls.push_back(&fall);
    return falls;
}

void draw_waterfalls(ModContext*, const GfxStageContext* context, void*) {
    waterfall_contact_ready = false;
    if (!context || !context->game_view || !settings::enhanced_water_enabled()) return;
    const auto falls = loaded_falls();
    if (falls.empty()) return;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &waterfall_camera) != MOD_OK) return;
    waterfall::draw(falls, waterfall_camera);
    const auto* viewport = static_cast<const view_port_class*>(context->game_viewport);
    if (!viewport || viewport->x_orig != 0 || viewport->y_orig != 0 ||
        !(viewport->width > 0 && viewport->height > 0)) return;
    const auto& clip = viewport->scissor;
    waterfall_clip = {std::clamp(clip.x_orig / viewport->width, 0.0f, 1.0f),
                      std::clamp(clip.y_orig / viewport->height, 0.0f, 1.0f),
                      std::clamp((clip.x_orig + clip.width) / viewport->width, 0.0f, 1.0f),
                      std::clamp((clip.y_orig + clip.height) / viewport->height, 0.0f, 1.0f)};
    waterfall_contact_ready = waterfall_clip.left < waterfall_clip.right && waterfall_clip.top < waterfall_clip.bottom;
}

void draw_waterfall_contact(ModContext*, const GfxStageContext*, void*) {
    if (!waterfall_contact_ready) return;
    waterfall_contact_ready = false;
    const auto falls = loaded_falls();
    waterfall::draw(falls, waterfall_camera, &waterfall_clip);
}

void stage(ModContext*, const GfxStageContext* context, void*) {
    const auto* scene = dComIfGp_getStartStageName();
    if (!context || !context->game_view || !scene) return;
    if (cached_stage != scene) {
        clear_cover();
        cached_stage = scene;
    }
    const int stay = dComIfGp_roomControl_getStayNo();
    struct BackgroundPart {
        J3DModel* model = nullptr;
        bool animated = false;
    };
    std::array<std::array<BackgroundPart, PartsPerRoom>, RoomSlots> models{};
    (void)fopAcIt_Executor([](void* raw, void* output) -> int {
        if (!raw || fopAcM_GetName(raw) != fpcNm_BG_e) return 0;
        auto* actor = static_cast<daBg_c*>(raw);
        const int room = fopAcM_GetRoomNo(actor);
        if (room >= 0 && room < int(RoomSlots) && dComIfGp_roomControl_checkStatusFlag(room, 0x10)) {
            auto& parts = (*static_cast<decltype(models)*>(output))[room];
            for (unsigned part = 0; part < PartsPerRoom; ++part)
                parts[part] = {actor->mBgParts[part].model, part != 0 && actor->mBgParts[part].btk};
        }
        return 0;
    }, &models);
    const unsigned first = stay >= 0 && stay < int(RoomSlots) ? unsigned(stay) : 0;
    bool built = false;
    for (unsigned offset = 0; offset < RoomSlots; ++offset) {
        const unsigned id = (first + offset) % RoomSlots;
        for (unsigned part = 0; part < PartsPerRoom; ++part) {
            auto& cover = room_cover[id][part];
            auto* instance = models[id][part].model;
            auto* model = instance ? instance->getModelData() : nullptr;
            if (cover.instance == instance && cover.model == model) continue;
            cover = {};
            if (!model || built) continue;
            cover.instance = instance;
            cover.model = model;
            frame_reported = false;
            scatter(*instance, int(id), part, part == 0 && id == 0 && cached_stage == "F_SP103",
                    models[id][part].animated, cover);
            built = true;
        }
    }
    update_object_falls();
    if (!settings::ground_grass_enabled()) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    clumps.clear();
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
    for (unsigned offset = 0; offset < RoomSlots && clumps.size() < MaxVisible; ++offset) {
        for (const auto& cover : room_cover[(first + offset) % RoomSlots]) {
            for (const auto& tile : cover.grass_tiles) {
                if (clumps.size() >= MaxVisible) break;
                const float x = std::clamp(camera.eye[0], tile.x, tile.x + GrassTileSize) - camera.eye[0];
                const float z = std::clamp(camera.eye[2], tile.z, tile.z + GrassTileSize) - camera.eye[2];
                if (x*x + z*z >= DrawDistance * DrawDistance) continue;
                for (const auto& root : tile.roots) {
                    if (clumps.size() >= MaxVisible) break;
                    const float dx = root.point.x - camera.eye[0], dz = root.point.z - camera.eye[2];
                    const float dy = root.point.y - camera.eye[1];
                    const float distance2 = dx*dx + dy*dy + dz*dz;
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
            }
        }
    }
    const auto grass_count = clumps.size();
    std::array<unsigned, static_cast<unsigned>(DetailStyle::Count)> detail_counts{};
    const auto ambient = g_env_light.bg_amb_col[0];
    const auto terrain_light = [](float channel) {
        return std::clamp(0.50f + 0.50f * channel / 255.0f, 0.50f, 1.0f);
    };
    const Vec3 light{terrain_light(ambient.r), terrain_light(ambient.g), terrain_light(ambient.b)};
    const bool twilight = dKy_darkworld_check();
    for (unsigned offset = 0; offset < RoomSlots && clumps.size() - grass_count < MaxPebbles; ++offset) {
        for (const auto& cover : room_cover[(first + offset) % RoomSlots]) {
            for (const auto& pebble : cover.pebbles) {
                if (clumps.size() - grass_count >= MaxPebbles) break;
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
                if (!frame_reported) ++detail_counts[static_cast<unsigned>(pebble.slope.z)];
            }
        }
    }
    if (!frame_reported && !clumps.empty()) {
        char message[200];
        std::snprintf(message, sizeof(message),
                      "Ground cover visible: %zu grass roots, %u stones, %u leaves, %u twigs, %u plants, %u moss patches",
                      grass_count, detail_counts[0], detail_counts[1], detail_counts[2], detail_counts[3],
                      detail_counts[4]);
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
        svc_log->warn(mod_ctx, "Ground cover: graphics or camera service unavailable");
        return;
    }
    waterfall::initialize();
    GfxDrawTypeDesc draw = GFX_DRAW_TYPE_DESC_INIT;
    draw.label = "MidnaFX ground cover";
    draw.draw = encode;
    if (svc_gfx->register_draw_type(mod_ctx, &draw, &draw_type) != MOD_OK)
        svc_log->warn(mod_ctx, "Ground cover: draw registration failed");
}

void update() {
    if (!draw_type) return;
    static bool was_ground = false, was_water = false;
    const bool ground = settings::ground_grass_enabled();
    const bool water = settings::enhanced_water_enabled();
    if (ground != was_ground || water != was_water) {
        clear_cover();
        was_ground = ground;
        was_water = water;
    }
    if (water && !waterfall_hook) {
        GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
        hook.callback = draw_waterfalls;
        (void)svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &hook, &waterfall_hook);
    } else if (!water && waterfall_hook) {
        svc_gfx->unregister_stage_hook(mod_ctx, waterfall_hook);
        waterfall_hook = 0;
    }
    if (water && !waterfall_contact_hook) {
        GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
        hook.callback = draw_waterfall_contact;
        (void)svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &hook, &waterfall_contact_hook);
    } else if (!water && waterfall_contact_hook) {
        svc_gfx->unregister_stage_hook(mod_ctx, waterfall_contact_hook);
        waterfall_contact_hook = 0;
        waterfall_contact_ready = false;
    }
    const bool wanted = ground || water;
    if (wanted == (stage_hook != 0)) return;
    if (!wanted) {
        svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
        stage_hook = 0;
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
    if (waterfall_hook) svc_gfx->unregister_stage_hook(mod_ctx, waterfall_hook);
    if (waterfall_contact_hook) svc_gfx->unregister_stage_hook(mod_ctx, waterfall_contact_hook);
    waterfall_contact_hook = 0;
    waterfall_contact_ready = false;
    waterfall::shutdown();
    stage_hook = waterfall_hook = draw_type = 0;
    if (pipeline) wgpuRenderPipelineRelease(pipeline);
    pipeline = nullptr;
    clear_cover();
    frame_reported = false;
    std::vector<Clump>().swap(clumps);
}
} // namespace midnafx::render::ground_grass
