#include "ground_grass.hpp"
#include "grass_view.hpp"
#include "grass_reach.hpp"
#include "grass_assembly.hpp"
#include "airborne_source_cache.hpp"
#include "immutable_instances.hpp"
#include "terrain_index.hpp"
#include "waterfall.hpp"
#include "game/topology.hpp"
#include "game/wind.hpp"
#include "game/water_probe.hpp"
#include "game/terrain_interaction.hpp"
#include "midnafx_shader.hpp"
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
#include <bit>
#include <cmath>
#include <cctype>
#include <chrono>
#include <string>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace midnafx::render::ground_grass {
namespace {
using topology::Vec3;
constexpr unsigned MaxVisible = 28000;
constexpr unsigned MaxPebbles = 24000;
constexpr float PebbleDistance = 1800.0f;
constexpr unsigned MaxPlants = 12000;
constexpr unsigned MaxVisiblePlants = 6000;
constexpr float PlantDistance = 5200.0f;
constexpr unsigned BladesPerClump = 9;
constexpr unsigned VerticesPerClump = BladesPerClump * 3;
constexpr float DrawDistance = 2200.0f;
constexpr float FarGrassDistance = 10000.0f;
constexpr float FarGrassTileSize = 1024.0f;
constexpr unsigned MaxFarGrassVisible = 24000;
constexpr unsigned MaxCanopyVisible = 24000;
constexpr unsigned MaxCanopyTiles = 192;
float canopy_reach = FarGrassDistance;
float canopy_tile_size = 2048;
constexpr float PathMargin = 18.0f;
constexpr float PathCell = 128.0f;
constexpr float GrassTileSize = 256.0f;
constexpr unsigned MaxGrassTiles = 256;
constexpr unsigned MaxFarGrassTiles = 384;
constexpr unsigned GrassTilesPerFrame = 8;

struct Root {
    Vec3 point;
    Vec3 color;
    float height, width, angle;
};
enum class DetailStyle { Stone, Leaf, Twig, Plant, Moss, Count };
struct Pebble {
    Root root;
    Vec3 slope; // XY hold ground slope, Z holds DetailStyle.
    bool forest = false;
};
struct Clump {
    Vec3 point;
    Vec3 shape; // Negative height marks detail; negative angle marks distant grass.
    Vec3 base;  // Grass base color or pebble surface slope in XY.
    Vec3 tip;
    Vec3 trail{};
};
struct CoverUniforms {
    float projection[16];
    std::array<float, 4> wind;
    terrain_interaction::Sources actors;
    std::array<float, 4> ambient, light_position, direct, eye, fog_color, fog_range;
};
static_assert(sizeof(CoverUniforms) == 208);
struct Draw {
    GfxRange instances;
    GfxRange projection;
    unsigned count;
    immutable_instances::Resource* immutable = nullptr;
};
static_assert(sizeof(Draw) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
GfxDeviceInfo grass_device = GFX_DEVICE_INFO_INIT;
immutable_instances::Pool instance_pool;

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
    unsigned used = 0;
};
struct GrassSurface {
    std::array<Vec3, 3> points, uv;
    const u8* image;
    unsigned width, height, format;
    float min_x, max_x, min_z, max_z;
    bool ignore_alpha;
};
struct RoomCover {
    const J3DModel* instance = nullptr;
    const J3DModelData* model = nullptr;
    std::vector<GrassSurface> grass_surfaces;
    TerrainIndex grass_index;
    grass_reach::Points reach_points;
    std::vector<std::array<Vec3, 3>> grass_paths;
    std::array<std::unordered_map<std::uint64_t, GrassTile>, 3> grass_tiles;
    std::vector<Pebble> pebbles;
    std::vector<Pebble> plants;
    std::vector<waterfall::Fall> falls;
};
constexpr unsigned RoomSlots = 64;
constexpr unsigned PartsPerRoom = 6;
std::array<std::array<RoomCover, PartsPerRoom>, RoomSlots> room_cover;
std::string cached_stage;
unsigned grass_revision = 0;
grass_reach::Cache reach_cache;
grass_assembly::Cache assembly_cache;
airborne_sources::Cache<AirborneSource> airborne_cache;
struct ObjectFall {
    RoomCover cover;
    float transform[12]{};
    bool present = false;
};
std::unordered_map<const J3DModel*, ObjectFall> object_falls;
std::vector<Clump> clumps;
// Borrowed tile pointers are cleared before each query and on cover invalidation.
std::array<std::vector<const GrassTile*>, 3> visible_tiles;
bool frame_reported = false;
terrain_trails::History trail_history;
bool trail_reported = false;
// Same diagnostics toggle as the final renderer. Capture bounded CPU spans only;
// serialization happens in mod_update, never in the timed stage callback.
struct GrassCpuProfile {
    using Clock = std::chrono::steady_clock;
    struct Sample {
        std::array<double, 5> us{};
        std::array<float, 2> eye{};
        float furthest = 2200;
        unsigned roots = 0;
        bool ready = false, assembly_reused = false, proof_recorded = false;
        std::uint64_t geometry_hash = 0, input_hash = 0;
        std::uint64_t instance_hash = 0, uploaded_bytes = 0, resident_bytes = 0;
        bool immutable_upload = false;
        double discovery_us = 0, airborne_us = 0;
        unsigned airborne_count = 0;
        bool airborne_sampled = false, airborne_reused = false;
        std::uint64_t airborne_hash = 0;
    };
    std::array<Sample, 384> samples{};
    Sample current{};
    Clock::time_point previous{};
    unsigned count = 0;
    bool active = false, reported = false;
    void reset() { count = 0; active = reported = false; }
    void begin(bool enabled, const float* eye) {
        if (!enabled) { reset(); return; }
        active = count < samples.size();
        if (active) { current = {}; current.eye = {eye[0], eye[2]}; previous = Clock::now(); }
    }
    void mark(unsigned phase) {
        if (!active) return;
        const auto now = Clock::now();
        current.us[phase] = std::chrono::duration<double, std::micro>(now - previous).count();
        previous = now;
    }
    void finish(unsigned roots, bool ready) {
        if (!active) return;
        mark(4); current.roots = roots; current.ready = ready;
        samples[count++] = current; active = false;
    }
    Sample* airborne_target() {
        if (reported || count == 0 || samples[count-1].airborne_sampled) return nullptr;
        return &samples[count-1];
    }
    void finish_airborne(Sample* sample, Clock::time_point start, std::span<const AirborneSource> sources, bool reused) {
        if (!sample) return;
        sample->airborne_us = std::chrono::duration<double, std::micro>(Clock::now()-start).count();
        sample->airborne_count = static_cast<unsigned>(sources.size());
        sample->airborne_sampled = true; sample->airborne_reused = reused;
        // Ordered source proofs outside the measured interval, at the same
        // transition/settled indices as the instance geometry proofs.
        const auto index = static_cast<unsigned>(sample - samples.data());
        if (index != 128 && index != 129 && index != samples.size()-1) return;
        sample->airborne_hash = 14695981039346656037ull;
        const auto append = [&](std::uint32_t value) { sample->airborne_hash = (sample->airborne_hash ^ value)*1099511628211ull; };
        for (const auto& source : sources) {
            for (float value : source.point) append(std::bit_cast<std::uint32_t>(value));
            append(source.forest ? 1u : 0u);
        }
    }
    void proof(const CameraInfo& camera, float reach, unsigned first, const std::vector<Clump>& instances) {
        // Transition and settled-camera proofs, outside all captured CPU spans. Hash immutable
        // instance fields only; trails deliberately remain a live per-frame input.
        if ((count != 129 && count != 130 && count != samples.size()) || !samples[count-1].ready) return;
        auto& sample = samples[count-1];
        if (sample.proof_recorded) return;
        sample.proof_recorded = true;
        const auto append = [](std::uint64_t& hash, std::uint32_t word) { hash = (hash ^ word) * 1099511628211ull; };
        sample.geometry_hash = sample.input_hash = sample.instance_hash = 14695981039346656037ull;
        for (auto word : grass_assembly::key(camera, reach, 0, first).camera) append(sample.input_hash, word);
        append(sample.input_hash, first);
        for (const auto& instance : instances) {
            for (const auto& value : {instance.point, instance.shape, instance.base, instance.tip})
                for (auto word : std::bit_cast<std::array<std::uint32_t, 3>>(value)) {
                    append(sample.geometry_hash, word); append(sample.instance_hash, word);
                }
            for (auto word : std::bit_cast<std::array<std::uint32_t, 3>>(instance.trail)) append(sample.instance_hash, word);
        }
    }
    void report() {
        if (reported || count != samples.size() || !svc_log) return;
        reported = true;
        for (unsigned i = 0; i < count; ++i) {
            const auto& sample = samples[i];
            char message[1024];
            std::snprintf(message, sizeof(message),
                "Grass CPU: {\"schema\":1,\"sample\":%u,\"us\":[%.3f,%.3f,%.3f,%.3f,%.3f],\"roots\":%u,\"ready\":%s,\"eye_xz\":[%.9g,%.9g],\"furthest\":%.9g,\"input_hash\":\"%016llx\",\"geometry_hash\":\"%016llx\",\"assembly_reused\":%s,\"instance_hash\":\"%016llx\",\"uploaded_bytes\":%llu,\"resident_bytes\":%llu,\"immutable_upload\":%s,\"discovery_us\":%.3f,\"airborne_us\":%.3f,\"airborne_count\":%u,\"airborne_sampled\":%s,\"airborne_reused\":%s,\"airborne_hash\":\"%016llx\"}",
                i, sample.us[0], sample.us[1], sample.us[2], sample.us[3], sample.us[4],
                sample.roots, sample.ready ? "true" : "false", sample.eye[0], sample.eye[1], sample.furthest,
                static_cast<unsigned long long>(sample.input_hash), static_cast<unsigned long long>(sample.geometry_hash),
                sample.assembly_reused ? "true" : "false", static_cast<unsigned long long>(sample.instance_hash),
                static_cast<unsigned long long>(sample.uploaded_bytes), static_cast<unsigned long long>(sample.resident_bytes),
                sample.immutable_upload ? "true" : "false", sample.discovery_us, sample.airborne_us,
                sample.airborne_count, sample.airborne_sampled ? "true" : "false", sample.airborne_reused ? "true" : "false",
                static_cast<unsigned long long>(sample.airborne_hash));
            svc_log->info(mod_ctx, message);
        }
    }
} cpu_profile;
Vec3 trail_bend(Vec3 point) {
    const auto effect=trail_history.sample({point.x,point.y,point.z});
    return {effect[0],effect[1],effect[2]};
}

constexpr char Shader[] = R"(
struct Output {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
    @location(1) world: vec3f,
    @location(2) @interpolate(flat) kind: u32,
}
struct CoverUniforms {
    projection: mat4x4f,
    wind: vec4f,
    actors: array<vec4f, 2>,
    ambient: vec4f,
    light_position: vec4f,
    direct: vec4f,
    eye: vec4f,
    fog_color: vec4f,
    fog_range: vec4f,
}
@group(0) @binding(0) var<uniform> scene: CoverUniforms;
override blades_per_clump: f32;
fn cover_vertex(point: vec3f, color: vec3f, kind: u32) -> Output {
    return Output(scene.projection * vec4f(point, 1.0), color, point, kind);
}
fn blade_variation(seed: u32) -> vec3f {
    var bits = (seed ^ (seed >> 16u)) * 0x7feb352du;
    bits = (bits ^ (bits >> 15u)) * 0x846ca68bu;
    bits ^= bits >> 16u;
    return vec3f(f32(bits & 255u), f32((bits >> 8u) & 255u),
                 f32((bits >> 16u) & 255u)) / 255.0;
}
fn actor_bend(root: vec3f) -> vec3f {
    var bend = vec3f(0.0);
    for (var i = 0u; i < 2u; i++) {
        let actor = scene.actors[i];
        if (actor.w <= 0.0 || abs(root.y - actor.y) > 100.0) { continue; }
        let delta = root.xz - actor.xz;
        let distance = length(delta);
        let weight = 1.0 - smoothstep(actor.w * 0.18, actor.w, distance);
        bend += vec3f(delta / max(distance, 1.0) * weight, weight);
    }
    let amount = clamp(bend.z, 0.0, 1.0);
    return vec3f(bend.xy / max(length(bend.xy), 1.0) * amount, amount);
}
fn contact_bend(root: vec3f, trail: vec3f) -> vec3f {
    let current = actor_bend(root);
    let direction = current.xy + trail.xy * (1.0 - current.z);
    return vec3f(direction / max(length(direction), 1.0), max(current.z, trail.z));
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
    let point = root + vec3f(offset.x, rise + dot(slope, offset), offset.y);
    return cover_vertex(point, color * select(0.83, 1.0, corner == 2u), 1u);
}
fn litter_vertex(index: u32, root: vec3f, shape: vec3f, surface: vec3f, color: vec3f, trail: vec3f) -> Output {
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
            return cover_vertex(root, color, 3u);
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
        // Asymmetric leaflets share a root and vary their proportions independently.
        let leaflet = facet / 3u;
        let face = facet % 3u;
        let verts = array<vec3f, 5>(vec3f(0.0, 0.0, 0.10), vec3f(0.48, -0.28, 0.31),
                                   vec3f(0.50, 0.0, 0.525), vec3f(0.52, 0.27, 0.34),
                                   vec3f(1.0, 0.0, 0.95));
        let faces = array<vec3u, 3>(vec3u(0, 1, 2), vec3u(1, 4, 2), vec3u(0, 4, 3));
        let vert = verts[faces[face][corner]];
        let leaf = blade_variation(seed + leaflet);
        point = vec2f(vert.x * mix(0.48, 1.0, leaf.x),
                      vert.y * mix(0.42, 1.22, variation.y) + vert.x * vert.x * (leaf.y - 0.5) * 0.34);
        height += -shape.x * vert.z * mix(0.38, 1.18, leaf.z);
        angle += f32(leaflet) * 2.0943951 + (leaf.y - 0.5) * 1.55;
        shade = select(0.78, 1.02, face == 2u) * mix(0.87, 1.08, leaf.x);
        if (variation.x < 0.38) {
            // Nine tapered leaflets form a taller, irregular fern-like clump.
            let row = facet / 2u;
            let leaflet = blade_variation(seed + facet);
            let side = select(-1.0, 1.0, facet % 2u == 0u);
            let reach = mix(0.45, 0.88, leaflet.x) * (1.0 - f32(row) * 0.11);
            point = vec2f(select(0.10, reach, corner == 2u),
                          select(-0.055, 0.055, corner == 1u));
            height = 0.5 + -shape.x * (0.18 + f32(row) * 0.19 +
                                      select(0.0, 0.16, corner == 2u));
            angle = shape.z + side * mix(0.65, 1.25, leaflet.y);
            shade = mix(0.79, 1.06, leaflet.z);
        }
    } else {
        // A scalloped cushion hugs the slope with a raised, softly shaded crown.
        let rim = (facet + corner) % 9u;
        let theta = f32(rim) * 0.6981317;
        let jitter = blade_variation(seed + rim);
        if (corner < 2u) {
            point = vec2f(cos(theta), sin(theta) * mix(0.58, 0.88, variation.y)) *
                    mix(0.65, 1.0, jitter.x);
            shade = mix(0.83, 0.98, jitter.y);
        } else {
            height += -shape.x * mix(0.65, 1.0, variation.z);
            shade = 1.04;
        }
    }
    let axis = vec2f(cos(angle), sin(angle));
    var offset = shape.y * vec2f(point.x * axis.x - point.y * axis.y, point.x * axis.y + point.y * axis.x);
    if (style == 3u) {
        let flex = clamp((height - 0.5) / max(-shape.x, 0.001), 0.0, 1.0);
        offset += scene.wind.xy * wind_gust(root.xz, scene.wind) * min(8.0, -shape.x * 0.8) * flex * flex;
        let contact = contact_bend(root, trail);
        offset += contact.xy * min(shape.y, 14.0) * flex * flex;
        height = mix(height, 0.5 + (height - 0.5) * 0.45, contact.z);
    }
    let position = root + vec3f(offset.x, height + dot(surface.xy, offset), offset.y);
    return cover_vertex(position, color * shade, style + 1u);
}
@vertex fn vs_main(@builtin(vertex_index) index: u32,
                   @location(0) root: vec3f, @location(1) shape: vec3f,
                   @location(2) base: vec3f, @location(3) tip: vec3f,
                   @location(4) trail: vec3f) -> Output {
    if (shape.x < 0.0) {
        if (base.z > 0.5) {
            return litter_vertex(index, root, shape, base, tip, trail);
        }
        return pebble_vertex(index, root, shape, base.xy, tip);
    }
    let blade = f32(index / 3u);
    let corner = index % 3u;
    let variation = blade_variation(bitcast<u32>(shape.z) + index / 3u);
    let angle = abs(shape.z) + (blade + (variation.x - 0.5) * 2.8) * (6.2831853 / blades_per_clump);
    let radial = vec2f(cos(angle), sin(angle));
    let canopy = shape.z < 0.0;
    let spread = min(select(8.5, shape.y * 0.8, canopy), shape.y * mix(0.19, 0.73, variation.y));
    let width = shape.y * mix(0.065, 0.135, variation.z);
    var offset = radial * spread;
    var height = 0.5;
    var color = base;
    if (corner == 2u) {
        height = shape.x * mix(0.46, 1.08, variation.z);
        let gust = wind_gust(root.xz, scene.wind);
        let rest = radial * spread * (1.0 - gust * 0.65);
        let bend = scene.wind.xy * min(height * 0.94, 10.0) * gust;
        let lean = radial * spread + rest + bend;
        let reach = select(17.0, shape.y * 1.4, canopy);
        offset = lean * min(1.0, reach / max(length(lean), 0.001));
        height *= 1.0 - 0.26 * gust * gust;
        let contact = contact_bend(root, trail);
        offset += contact.xy * min(shape.x, 16.0);
        offset *= min(1.0, reach / max(length(offset), 0.001));
        height *= 1.0 - contact.z * 0.62;
        color = tip;
    } else {
        let side = select(-width, width, corner == 0u);
        offset += vec2f(-radial.y, radial.x) * side;
    }
    return cover_vertex(root + vec3f(offset.x, height, offset.y), color, select(0u, 6u, shape.z < 0.0));
}
@fragment fn fs_main(in: Output) -> @location(0) vec4f {
    // Actual deformed geometry supplies the face normal for every detail class.
    let perpendicular = cross(dpdx(in.world), dpdy(in.world));
    let magnitude = dot(perpendicular, perpendicular);
    var normal = vec3f(0.0, 1.0, 0.0);
    if (magnitude > 1e-12) { normal = perpendicular * inverseSqrt(magnitude); }
    normal = select(normal, -normal, normal.y < 0.0);
    let ray = scene.light_position.xyz - in.world;
    let direction = ray / max(length(ray), 0.001);
    let leaf = in.kind == 0u || in.kind == 2u || in.kind == 4u || in.kind == 6u;
    // Thin foliage transmits light from its reverse side; stone stays opaque.
    let incidence = select(max(dot(normal, direction), 0.0),
                           0.28 + 0.72 * abs(dot(normal, direction)), leaf);
    // Subpixel distant blades use a broad canopy response instead of flickering
    // between bright and dark individual facets.
    let diffuse = select(incidence, 0.85, in.kind == 6u);
    var color = in.color * (scene.ambient.rgb + scene.direct.rgb * diffuse);
    let eye_ray = scene.eye.xyz - in.world;
    let eye = eye_ray / max(length(eye_ray), 0.001);
    let half_ray = direction + eye;
    let half_vector = half_ray / max(length(half_ray), 0.001);
    let gloss = select(select(0.0, 0.025, leaf && in.kind != 6u), 0.065, in.kind == 1u);
    let highlight = pow(max(dot(normal, half_vector), 0.0), 24.0) * gloss *
                      max(dot(normal, direction), 0.0);
    color += scene.direct.rgb * highlight;
    // Fog follows lighting so distant detail converges to the native fog color.
    let haze = clamp((length(eye_ray) - scene.fog_range.x) / scene.fog_range.y, 0.0, 1.0);
    return vec4f(mix(clamp(color, vec3f(0.0), vec3f(1.0)), scene.fog_color.rgb, haze), 1.0);
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

Vec3 surface_color(const u8* image, unsigned width, unsigned height, unsigned format, float u, float v,
                   bool* opaque = nullptr) {
    if (!std::isfinite(u) || !std::isfinite(v)) {
        if (opaque) *opaque = false;
        return {};
    }
    const auto x = std::min(width - 1, static_cast<unsigned>((u - std::floor(u)) * width));
    const auto y = std::min(height - 1, static_cast<unsigned>((v - std::floor(v)) * height));
    if (format != GX_TF_CMPR) {
        const auto index = (y % 4 * 4 + x % 4) * 2;
        const auto tile = (y / 4) * ((width + 3) / 4) + x / 4;
        if (format == GX_TF_RGBA8) {
            const auto* block = image + tile * 64;
            if (opaque) *opaque = block[index] >= 128;
            return {block[index + 1] / 255.0f, block[index + 32] / 255.0f, block[index + 33] / 255.0f};
        }
        const auto color = be16(image + tile * 32 + index);
        if (opaque) *opaque = true;
        if (format == GX_TF_RGB565) return rgb565(color);
        if (color & 0x8000) return {float((color >> 10) & 31) / 31, float((color >> 5) & 31) / 31,
                                    float(color & 31) / 31};
        if (opaque) *opaque = (color >> 12) >= 4;
        return {float((color >> 8) & 15) / 15, float((color >> 4) & 15) / 15, float(color & 15) / 15};
    }
    const auto* block = image + ((y / 8) * ((width + 7) / 8) + x / 8) * 32 +
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
    if (has_any(material, {"tuchimichi", "tutimiti", "sunamichi", "road_tsuchi", "tuchi_michi",
                           "tsuchimichi", "tutimichi", "_road_", "_path_"}) ||
        has_any(texture, {"tuchimichi", "tutimiti", "tsuchimichi", "sunamichi"}))
        return Cover::Path;
    if (has_any(texture, {"groundmoriwood", "tuchiwood", "c_a_hm_tuti", "c_hm_tuti", "ochiba",
                          "forestfloor", "forest_floor", "leaflitter", "leaf_litter"}) ||
        (texture == "c_woodsita" && has_any(material, {"kiwatuti", "ground", "jimen"})))
        return Cover::Litter;
    if ((has_any(material, {"iwakusa", "iwa_kusa", "kusaiwa", "moss", "kokeground", "aa_koke"}) ||
         (material == "aa_siba_v" && texture == "kkoke02")) &&
        !has_any(material, {"wood", "kikoke", "pole", "kokeita"}) &&
        has_any(texture, {"kusa", "moss", "koke"}))
        return Cover::Moss;
    if (!has_any(material, {"blend", "kusaset", "kiwa"}) &&
        has_any(material, {"kusa", "grass", "siba", "shiba"}) &&
        has_any(texture, {"kusa", "grass", "siba", "shiba"}) &&
        !has_any(texture, {"wall", "kabe", "nuki", "kusari", "kusaset"}))
        return Cover::Grass;
    if (has_any(material, {"gravel", "rock_ground", "rockground"}) ||
        (material.starts_with("aa_") &&
         has_any(material, {"jari", "tuchi_v", "tuti_v", "tsuchi_v",
                            "tuchi_a_v", "tuti00_v", "tuti01_v", "tuchi128"})) ||
        material == "dd_ma00_tuchi_v_x")
        return Cover::Gravel;
    return Cover::None;
}

bool waterfall_sheet(std::string_view name) {
    return name == "cf_ma03_water02_v" || name == "cf_ma03_water02_v_x" ||
           has_any(name, {"_taki_", "_takikasan_", "_waterkasan_", "_nigorikasanwater_",
                          "_waterfall_", "_cascade_"});
}

void scatter(J3DModel& instance, int room, unsigned part, bool animated, RoomCover& cover,
             const dRes_info_c* object_archive = nullptr) {
    auto& pebbles = cover.pebbles;
    auto& model = *instance.getModelData();
    auto* names = model.getMaterialName();
    auto* textures = model.getTexture();
    auto* texture_names = model.getTextureName();
    if (!names || !textures || !texture_names) return;
    J3DMaterial* grass = nullptr;
    const u8* grass_image = nullptr;
    if (!object_archive && part == 0 && model.getMaterialNum() == 35 && model.getShapeNum() == 35 &&
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
    bool float_uv = false;
    for (unsigned i = 0; i < 32 && fmt[i].attr != GX_VA_NULL; ++i) {
        formats.push_back({u8(fmt[i].attr), u8(fmt[i].cnt), u8(fmt[i].type)});
        if (fmt[i].attr == GX_VA_TEX0 && fmt[i].cnt == GX_TEX_ST) {
            float_uv = fmt[i].type == GX_F32;
            if (fmt[i].type == GX_S16 && fmt[i].frac <= 15)
                uv_fraction = fmt[i].frac;
        }
    }

    if (uv_fraction != 9 || !contains(grass_image, 32768)) grass = nullptr;
    const bool ordon = grass != nullptr;
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
        if (float_uv) {
            float value[2];
            std::memcpy(value, uv + i * 8, sizeof(value));
            return Vec3{value[0], value[1], 0};
        }
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
            if (!waterfall_sheet(name)) continue;
            const bool cave = object_archive && name == "ce_ma03_waterkasan_v_x";
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
                char message[256];
                std::snprintf(message, sizeof(message),
                              "Waterfall: %s room %d part %u %s, %zu faces, %.0f x %.0f, foot %.0f %.0f %.0f",
                              cached_stage.c_str(), room, part, name.c_str(), fall.sheet.size(), fall.width, fall.height,
                              fall.foot.x, fall.foot.y, fall.foot.z);
                svc_log->info(mod_ctx, message);
            }
        }
    }
    const unsigned uv_stride = float_uv ? 8 : 4;
    if ((!settings::ground_grass_enabled() && !settings::atmosphere_enabled()) || animated || (!float_uv && uv_fraction < 0) ||
        data.getVtxArrStride(GX_VA_TEX0) != uv_stride || !contains(uv, uv_count * uv_stride)) return;

    struct Surface {
        Cover kind = Cover::None;
        const u8* image = nullptr;
        unsigned width = 0, height = 0, format = GX_TF_CMPR;
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
        if (surface.kind == Cover::Moss) {
            const auto* blend = material->getBlend();
            if (!blend || blend->getBlendMode() != GX_BM_NONE) {
                surface.kind = Cover::None;
                continue;
            }
        }
        auto* generator = material->getTexCoord(0);
        if (!generator || generator->getTexGenSrc() != GX_TG_TEX0) continue;
        const auto* texture = textures->getResTIMG(slot);
        const auto* image = textures->getImgDataPtr(slot);
        const unsigned width = texture->width, height = texture->height;
        const unsigned format = texture->format;
        if (!width || !height || (format != GX_TF_CMPR && format != GX_TF_RGB565 &&
                                  format != GX_TF_RGB5A3 && format != GX_TF_RGBA8)) continue;
        const unsigned block = format == GX_TF_CMPR ? 8 : 4;
        const auto bytes = std::size_t((width + block - 1) / block) * ((height + block - 1) / block) *
                           (format == GX_TF_RGBA8 ? 64 : 32);
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
        surface.format = format;
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

    for (const auto& path : paths)
        cover.grass_paths.push_back({to_world(path.a), to_world(path.b), to_world(path.c)});
    unsigned path_rejected = 0;
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
            const auto p = to_world(positions[a.position]), q = to_world(positions[b.position]),
                       r = to_world(positions[c.position]);
            if (std::abs(side(p, q, r)) < 1.0f) continue;
            cover.grass_surfaces.push_back({{p, q, r}, {read_uv(a.texcoord), read_uv(b.texcoord), read_uv(c.texcoord)},
                surface.image, surface.width, surface.height, surface.format,
                std::min({p.x, q.x, r.x}), std::max({p.x, q.x, r.x}),
                std::min({p.z, q.z, r.z}), std::max({p.z, q.z, r.z}), ordon});
        }
    }
    // Water/atmosphere also discover cover; they do not query grass tiles.
    // Toggling ground cover already invalidates RoomCover before enabling it.
    if (settings::ground_grass_enabled()) {
        cover.grass_index.rebuild(static_cast<unsigned>(cover.grass_surfaces.size()), [&](unsigned i) {
            const auto& surface = cover.grass_surfaces[i];
            return TerrainIndex::Bounds{surface.min_x, surface.max_x, surface.min_z, surface.max_z};
        });
        cover.reach_points.rebuild(cover.grass_surfaces.size() * 3, [&](std::size_t i) {
            const auto& point = cover.grass_surfaces[i / 3].points[i % 3];
            return grass_reach::Point{point.x, point.z};
        });
        if (settings::diagnostics_enabled() && svc_log && !cover.grass_surfaces.empty()) {
            char message[160];
            std::snprintf(message, sizeof(message), "Grass reach points: corners=%zu unique=%zu bytes=%zu",
                cover.grass_surfaces.size() * 3, cover.reach_points.size(), cover.reach_points.bytes());
            svc_log->info(mod_ctx, message);
        }
    }
    // Match authored ground materials. Overlay dirt gets fewer pebbles than base gravel.
    std::uint32_t rng = 0x5f27b6u;
    unsigned pebble_samples = 0, plant_samples = 0;
    for (u16 i = 0; i < model.getShapeNum(); ++i) {
        auto* shape = model.getShapeNodePointer(i);
        const auto& surface = surfaces[i];
        const bool path = surface.kind == Cover::Path;
        const bool litter = surface.kind == Cover::Litter;
        const bool moss = surface.kind == Cover::Moss;
        const bool meadow = surface.kind == Cover::Grass;
        if (!surface.image || (!path && !litter && !moss && !meadow && surface.kind != Cover::Gravel)) continue;

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

            const float sample_area = meadow ? 7200.0f : path ? 3800.0f : litter ? 1150.0f :
                                      moss ? 850.0f : 900.0f;
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
                auto color = surface_color(surface.image, surface.width, surface.height, surface.format,
                                           tex.x, tex.y, &opaque);
                if (!opaque) continue;
                const float green = carpet_weight(color);
                if ((moss && green < 0.38f) || (meadow && green < 0.42f) ||
                    (!moss && !litter && !meadow && green > 0.45f)) continue;
                if (litter || moss || meadow) {
                    const auto local = mix(positions[a.position], mix(positions[b.position], positions[c.position],
                                           v / std::max(u + v, 0.0001f)), u + v);
                    if (on_path(local)) continue;
                }
                auto style = DetailStyle::Stone;
                const float size_bias = random(rng);
                const float radius = std::lerp(2.2f, 8.5f, size_bias * size_bias);
                float height = radius * std::lerp(0.35f, 0.64f, random(rng));
                float width = radius;
                if (meadow) {
                    style = DetailStyle::Plant;
                    height = radius * 2.2f;
                    width = radius * 2.5f;
                    color = mix(color, {0.26f, 0.43f, 0.14f}, 0.33f);
                } else if (litter) {
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
                                    {slope_x, slope_z, static_cast<float>(style)}, litter};
                // Plants retain their own bounded reservoir, so large gravel or
                // litter surfaces cannot remove the room's vegetation samples.
                const bool plant = style == DetailStyle::Plant;
                auto& retained = plant ? cover.plants : pebbles;
                auto& samples = plant ? plant_samples : pebble_samples;
                const unsigned limit = plant ? MaxPlants : MaxPebbles;
                ++samples;
                if (retained.size() < limit) retained.push_back(pebble);
                else {
                    const auto replacement = unsigned(random(rng) * samples);
                    if (replacement < limit) retained[replacement] = pebble;
                }
            }
        }
    }
    char message[160];
    std::snprintf(message, sizeof(message), "Ground cover room %d part %u: %zu grass surfaces, %zu stones/litter, %zu plants, %zu path triangles",
                  room, part, cover.grass_surfaces.size(), pebbles.size(), cover.plants.size(), paths.size());
    svc_log->info(mod_ctx, message);
    if (path_rejected) {
        std::snprintf(message, sizeof(message), "Ground cover room %d part %u: %u path rejections", room, part, path_rejected);
        svc_log->info(mod_ctx, message);
    }
}

bool overlaps(const GrassSurface& surface, float x, float z, float size = GrassTileSize) {
    return surface.max_x >= x && surface.min_x <= x + size &&
           surface.max_z >= z && surface.min_z <= z + size;
}

GrassTile build_grass_tile(const RoomCover& cover, int tile_x, int tile_z, unsigned layer) {
    const float size = layer == 2 ? canopy_tile_size : layer ? FarGrassTileSize : GrassTileSize;
    const unsigned cells = layer == 2 ? 24 : 36;
    const float spacing = size / cells;
    const float margin = PathMargin + (layer ? spacing*2.0f : 0.0f);
    GrassTile tile{tile_x * size, tile_z * size, {}};
    std::vector<const GrassSurface*> surfaces;
    for (const auto& surface : cover.grass_surfaces)
        if (overlaps(surface, tile.x, tile.z, size)) surfaces.push_back(&surface);
    std::vector<PathTriangle> paths;
    for (const auto& points : cover.grass_paths) {
        const auto [a, b, c] = points;
        if (std::max({a.x, b.x, c.x}) < tile.x - margin ||
            std::min({a.x, b.x, c.x}) > tile.x + size + margin ||
            std::max({a.z, b.z, c.z}) < tile.z - margin ||
            std::min({a.z, b.z, c.z}) > tile.z + size + margin) continue;
        const float area = side(a, b, c);
        if (std::abs(area) < 1.0f) continue;
        paths.push_back({a, b, c, area > 0 ? 1.0f : -1.0f,
            margin * std::hypot(a.x - b.x, a.z - b.z),
            margin * std::hypot(b.x - c.x, b.z - c.z),
            margin * std::hypot(c.x - a.x, c.z - a.z)});
    }
    for (unsigned z = 0; z < cells; ++z) {
        for (unsigned x = 0; x < cells; ++x) {
            const auto gx = std::uint32_t(tile_x) * cells + x;
            const auto gz = std::uint32_t(tile_z) * cells + z;
            std::uint32_t seed = gx * 0x9e3779b9u ^ gz * 0x85ebca6bu;
            seed = (seed ^ (seed >> 16)) * 0x7feb352du;
            const float px = tile.x + (x + 0.04f + random(seed) * 0.92f) * spacing;
            const float pz = tile.z + (z + 0.04f + random(seed) * 0.92f) * spacing;
            for (const auto* surface : surfaces) {
                if (px < surface->min_x || px > surface->max_x ||
                    pz < surface->min_z || pz > surface->max_z) continue;
                const auto [a, b, c] = surface->points;
                const Vec3 point{px, 0, pz};
                const float area = side(a, b, c);
                const float u = side(a, point, c) / area;
                const float v = side(a, b, point) / area;
                if (u < 0 || v < 0 || u + v > 1) continue;
                const Vec3 root{px, a.y + (b.y - a.y) * u + (c.y - a.y) * v, pz};
                if (std::any_of(paths.begin(), paths.end(),
                                [&](const auto& path) { return path.covers(root); })) continue;
                const auto& uv = surface->uv;
                const float s = uv[0].x + (uv[1].x - uv[0].x) * u + (uv[2].x - uv[0].x) * v;
                const float t = uv[0].y + (uv[1].y - uv[0].y) * u + (uv[2].y - uv[0].y) * v;
                bool opaque;
                const auto color = surface_color(surface->image, surface->width, surface->height,
                                                  surface->format, s, t, &opaque);
                if (!opaque && !surface->ignore_alpha) continue;
                const float carpet = carpet_weight(color);
                const float patch = scatter_patch(root);
                const float coverage = std::lerp(0.07f, 1.0f, std::sqrt(carpet)) *
                                       std::lerp(std::lerp(0.62f, 1.0f, patch), 1.0f, carpet);
                if (random(seed) >= coverage) continue;
                tile.roots.push_back({root, color,
                    (std::lerp(15.0f, 5.0f, carpet) + 7 * random(seed)) * std::lerp(0.85f, 1.2f, patch),
                    std::lerp(2.4f, 13.2f, carpet) + 2 * random(seed), 6.283185f * random(seed)});
                if (tile.roots.size() >= cells * cells * 4) return tile;
            }
        }
    }
    if (layer) {
        // A stable spatial shuffle lets each tile share the distant draw budget
        // without spending it all on its first grid rows or the nearest terrain.
        const auto key = [](const Root& root) {
            auto hash = std::bit_cast<std::uint32_t>(root.point.x)*0x9e3779b9u ^
                        std::bit_cast<std::uint32_t>(root.point.z)*0x85ebca6bu;
            return (hash ^ (hash >> 16))*0x7feb352du;
        };
        std::sort(tile.roots.begin(), tile.roots.end(), [&](const auto& a, const auto& b) { return key(a) < key(b); });
    }
    return tile;
}

const std::array<std::vector<const GrassTile*>, 3>& prepare_grass_tiles(const CameraInfo& camera, bool& ready) {
    struct Request {
        RoomCover* cover;
        int x, z;
        float distance;
    };
    static std::array<std::vector<Request>, 3> requests;
    static std::array<float,3> previous_direction{};
    static float previous_fov = 0, previous_aspect = 0;
    static unsigned observed_revision = ~0u;
    static int previous_x = 0, previous_z = 0;
    const int camera_x = int(std::floor(camera.eye[0] / GrassTileSize));
    const int camera_z = int(std::floor(camera.eye[2] / GrassTileSize));
    const float furthest = reach_cache.get(camera.eye[0], camera.eye[2], grass_revision, [&] {
        float result = 2200;
        for (const auto& room : room_cover) for (const auto& cover : room)
            result = std::max(result, cover.reach_points.furthest(camera.eye[0], camera.eye[2]));
        return result;
    });
    cpu_profile.mark(0);
    if (cpu_profile.active) cpu_profile.current.furthest = furthest;
    const float next_reach = grass_view::reach(camera.far_plane, furthest, camera.fovy, camera.aspect);
    const float next_size = grass_view::tile_size(next_reach);
    if (next_size != canopy_tile_size) {
        for (auto& room : room_cover) for (auto& cover : room) cover.grass_tiles[2].clear();
        canopy_tile_size = next_size;
        observed_revision = ~0u;
    }
    canopy_reach = next_reach;
    // View changes matter: tiles behind the camera must not consume coverage.
    if (observed_revision != grass_revision || previous_x != camera_x || previous_z != camera_z ||
        previous_direction[0]*camera.view_from_world[2] + previous_direction[1]*camera.view_from_world[6] +
        previous_direction[2]*camera.view_from_world[10] < 0.9985f ||
        std::abs(previous_fov-camera.fovy) > 2 || std::abs(previous_aspect-camera.aspect) > 0.05f) {
        for (unsigned layer = 0; layer < 3; ++layer) {
            auto& layer_requests = requests[layer];
            layer_requests.clear();
            const float size = layer == 2 ? canopy_tile_size : layer ? FarGrassTileSize : GrassTileSize;
            const float radius = layer == 2 ? canopy_reach : layer ? FarGrassDistance : DrawDistance;
            const int min_x = int(std::floor((camera.eye[0] - radius) / size));
            const int max_x = int(std::floor((camera.eye[0] + radius) / size));
            const int min_z = int(std::floor((camera.eye[2] - radius) / size));
            const int max_z = int(std::floor((camera.eye[2] + radius) / size));
            for (auto& room : room_cover) {
                for (auto& cover : room) {
                    if (cover.grass_surfaces.empty()) continue;
                    for (int z = min_z; z <= max_z; ++z) {
                        for (int x = min_x; x <= max_x; ++x) {
                            const float px = x * size, pz = z * size;
                            const float dx = px + size * 0.5f - camera.eye[0];
                            const float dz = pz + size * 0.5f - camera.eye[2];
                            const float distance = dx * dx + dz * dz;
                            // Include tiles touching the draw circle, even when their center is outside it.
                            if (distance > (radius + size * 0.71f) * (radius + size * 0.71f) ||
                                !cover.grass_index.any({px, px+size, pz, pz+size}, [&](unsigned i) {
                                    const auto& surface = cover.grass_surfaces[i];
                                    float low_y=surface.points[0].y, high_y=low_y;
                                    for (const auto& p : surface.points) { low_y=std::min(low_y,p.y); high_y=std::max(high_y,p.y); }
                                    return grass_view::visible(camera.proj_from_world,
                                        {px,low_y,pz}, {px+size,high_y+40,pz+size});
                                })) continue;
                            layer_requests.push_back({&cover, x, z, distance});
                        }
                    }
                }
            }
            std::stable_sort(layer_requests.begin(), layer_requests.end(),
                             [](const auto& a, const auto& b) { return a.distance < b.distance; });
            const unsigned limit = layer == 2 ? MaxCanopyTiles : layer ? MaxFarGrassTiles : MaxGrassTiles;
            if (layer_requests.size() > limit) layer_requests.resize(limit);
        }
        observed_revision = grass_revision;
        previous_x = camera_x;
        previous_z = camera_z;
        previous_direction = {camera.view_from_world[2],camera.view_from_world[6],camera.view_from_world[10]};
        previous_fov=camera.fovy; previous_aspect=camera.aspect;
    }
    cpu_profile.mark(1);
    static unsigned epoch = 0;
    ++epoch;
    auto& tiles = visible_tiles;
    ready = true;
    for (unsigned layer = 0; layer < 3; ++layer) {
        unsigned built = 0;
        tiles[layer].clear();
        tiles[layer].reserve(requests[layer].size());
        for (const auto& request : requests[layer]) {
            auto& cache = request.cover->grass_tiles[layer];
            const auto key = path_key(request.x, request.z);
            auto found = cache.find(key);
            if (found == cache.end()) {
                // Separate budgets let distant coverage fill without delaying close blades.
                if (built >= (layer == 2 ? 3u : layer ? 4u : GrassTilesPerFrame)) continue;
                found = cache.emplace(key, build_grass_tile(*request.cover, request.x, request.z, layer)).first;
                ++built;
            }
            found->second.used = epoch;
            tiles[layer].push_back(&found->second);
        }
        for (auto& room : room_cover)
            for (auto& cover : room)
                std::erase_if(cover.grass_tiles[layer], [&](const auto& tile) { return tile.second.used != epoch; });
        ready &= tiles[layer].size() == requests[layer].size();
    }
    cpu_profile.mark(2);
    return tiles;
}

void encode(ModContext*, const GfxDrawContext* context, const void* payload, size_t size, void*) {
    if (size != sizeof(Draw)) return;
    Draw draw{};
    std::memcpy(&draw, payload, sizeof(draw));
    immutable_instances::Pending pending{draw.immutable};
    if (!context || context->layout.depth_stencil_format == WGPUTextureFormat_Undefined) return;
    if (!pipeline || pipeline_layout != context->layout.key || pipeline_reversed != context->uses_reversed_z) {
        if (pipeline) wgpuRenderPipelineRelease(pipeline);
        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
        const std::string shader_source = std::string(wind_shader) + Shader;
        source.code = {shader_source.data(), shader_source.size()};
        WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        shader_desc.nextInChain = &source.chain;
        auto shader = wgpuDeviceCreateShaderModule(context->device, &shader_desc);
        const size_t offsets[] = {offsetof(Clump, point), offsetof(Clump, shape),
                                  offsetof(Clump, base), offsetof(Clump, tip), offsetof(Clump, trail)};
        WGPUVertexAttribute attributes[5]{};
        for (unsigned i = 0; i < 5; ++i) {
            attributes[i].format = WGPUVertexFormat_Float32x3;
            attributes[i].offset = offsets[i];
            attributes[i].shaderLocation = i;
        }
        WGPUVertexBufferLayout buffer = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        buffer.arrayStride = sizeof(Clump);
        buffer.stepMode = WGPUVertexStepMode_Instance;
        buffer.attributeCount = 5;
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
    wgpuRenderPassEncoderSetVertexBuffer(context->pass, 0,
        draw.immutable ? draw.immutable->buffer : context->vertex_buffer,
        draw.immutable ? 0 : draw.instances.offset,
        draw.immutable ? draw.immutable->bytes : draw.instances.size);
    wgpuRenderPassEncoderDraw(context->pass, VerticesPerClump, draw.count, 0, 0);
    wgpuBindGroupRelease(group);
}

void clear_cover() {
    airborne_cache.reset();
    instance_pool.invalidate();
    reach_cache.reset();
    assembly_cache.reset();
    cpu_profile.reset();
    for (auto& tiles : visible_tiles) tiles.clear();
    trail_history.clear(); trail_reported=false;
    for (auto& room : room_cover) room = {};
    object_falls.clear();
    cached_stage.clear();
    ++grass_revision;
}

void update_object_falls() {
    if (!settings::enhanced_water_enabled()) return;
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
            bool has_sheet = false;
            for (u16 i = 0; i < model->getMaterialNum(); ++i) {
                if (waterfall_sheet(lowercase(names->getName(i)))) {
                    has_sheet = true;
                    break;
                }
            }
            if (!has_sheet) continue;
            const auto* archive = dComIfG_getObjectResInfo(daSetBgObj_c::getArcName(actor));
            if (archive) scatter(*instance, room, 0, true, entry.cover, archive);
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
    const bool profile_discovery = settings::diagnostics_enabled() && cpu_profile.count < cpu_profile.samples.size();
    const auto discovery_start = profile_discovery ? GrassCpuProfile::Clock::now() : GrassCpuProfile::Clock::time_point{};
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
            ++grass_revision;
            if (!model || built) continue;
            cover.instance = instance;
            cover.model = model;
            frame_reported = false;
            scatter(*instance, int(id), part, models[id][part].animated, cover);
            built = true;
        }
    }
    update_object_falls();
    if (!settings::ground_grass_enabled()) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    if (!std::all_of(std::begin(camera.eye), std::end(camera.eye),
                     [](float v) { return std::isfinite(v) && std::abs(v) < 1e7f; })) return;
    const auto* m = camera.proj_from_world;
    auto visible = [&](Vec3 point) {
        float clip[4];
        for (unsigned i = 0; i < 4; ++i)
            clip[i] = m[i]*point.x + m[4+i]*point.y + m[8+i]*point.z + m[12+i];
        return clip[3] > 0 && std::abs(clip[0]) <= clip[3] * 1.1f &&
               std::abs(clip[1]) <= clip[3] * 1.1f;
    };
    bool grass_ready = false;
    cpu_profile.begin(settings::diagnostics_enabled(), camera.eye);
    if (cpu_profile.active) cpu_profile.current.discovery_us =
        std::chrono::duration<double, std::micro>(cpu_profile.previous-discovery_start).count();
    const auto& tiles = prepare_grass_tiles(camera, grass_ready);
    struct ClearTilePointers {
        ~ClearTilePointers() { for (auto& list : visible_tiles) list.clear(); }
    } clear_tile_pointers;
    unsigned near_count = 0;
    unsigned distant_count = 0, canopy_count = 0, trampled = 0;
    std::size_t grass_count = 0, plant_start = 0;
    std::array<unsigned, static_cast<unsigned>(DetailStyle::Count)> detail_counts{};
    const auto assembly_key = grass_assembly::key(camera, canopy_reach, grass_revision, first);
    const bool reuse_assembly = frame_reported && assembly_cache.reusable(assembly_key, grass_ready);
    bool trails_changed = false;
    if (cpu_profile.active) cpu_profile.current.assembly_reused = reuse_assembly;
    if (reuse_assembly) {
        trampled = assembly_cache.refresh(clumps, trail_bend, &trails_changed);
    } else {
        clumps.clear();
        for (unsigned layer = 0; layer < 3; ++layer) {
            const auto start = clumps.size();
            const unsigned limit = layer == 2 ? MaxCanopyVisible : layer ? MaxFarGrassVisible : MaxVisible;
            const float reach = layer == 2 ? canopy_reach : layer ? FarGrassDistance : DrawDistance;
            const unsigned quota = layer ? std::max(1u, limit / std::max(1u, unsigned(tiles[layer].size()))) : limit;
            for (const auto* tile : tiles[layer]) {
                if (clumps.size() - start >= limit) break;
                unsigned tile_count = 0;
                for (const auto& root : tile->roots) {
                    if (clumps.size() - start >= limit) break;
                    if (tile_count >= quota) break;
                    const float dx = root.point.x - camera.eye[0], dz = root.point.z - camera.eye[2];
                    const float dy = root.point.y - camera.eye[1];
                    const float distance2 = dx*dx + dy*dy + dz*dz;
                    if (distance2 >= reach * reach || !visible(root.point)) continue;
                    const float distance = std::sqrt(distance2);
                    const float density_radius = layer ? 6500.0f : 650.0f;
                    float density = std::min(1.0f, density_radius * density_radius / std::max(distance2, 1.0f));
                    if (layer == 2) {
                        const float blend = std::clamp((distance-6000.0f)/2200.0f, 0.0f, 1.0f);
                        density = blend * blend * (3-2*blend);
                    } else if (layer) {
                        const float blend = std::clamp((distance - 1200.0f) / 800.0f, 0.0f, 1.0f);
                        density *= blend * blend * (3.0f - 2.0f * blend);
                    }
                    std::uint32_t seed = std::bit_cast<std::uint32_t>(root.point.x) * 0x9e3779b9u ^
                                         std::bit_cast<std::uint32_t>(root.point.z) * 0x85ebca6bu;
                    const float weight = std::clamp((density - random(seed)) * 16.0f, 0.0f, 1.0f);
                    const float height = (layer == 2 ? std::max(root.height*1.6f, distance*0.0009f) : root.height * (layer ? 1.6f : 1.25f)) * weight *
                                         std::clamp((reach - distance) / (layer == 2 ? 512.0f : layer ? 1400.0f : 450.0f), 0.0f, 1.0f);
                    if (height < 1.0f) continue;
                    const Vec3 base = scale(root.color, 0.88f);
                    const Vec3 tip = root.color;
                    const float footprint = layer ? std::clamp(std::sqrt(float(tile->roots.size())/quota), 1.0f, 2.0f) : 1;
                    const float width = (layer == 2 ? std::max(root.width*1.8f, canopy_tile_size/24*0.65f) : root.width*(layer ? 1.8f : 1.0f)) * footprint;
                    const auto trail=layer==0 ? trail_bend(root.point) : Vec3{};
                    trampled+=trail.z>0.05f;
                    clumps.push_back({root.point, {height, width, layer == 2 ? -root.angle-8.0f : layer ? -root.angle - 0.01f : root.angle}, base, tip, trail});
                    ++tile_count;
                }
            }
            if (!layer) near_count = static_cast<unsigned>(clumps.size());
            if (layer == 1) distant_count = unsigned(clumps.size()-start);
            if (layer == 2) canopy_count = unsigned(clumps.size()-start);
        }
        grass_count = clumps.size();
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
                    clumps.push_back({root.point, {-root.height * fade, root.width * fade, root.angle},
                                      pebble.slope, root.color});
                    if (!frame_reported) ++detail_counts[static_cast<unsigned>(pebble.slope.z)];
                }
            }
        }
        plant_start = clumps.size();
        for (unsigned offset = 0; offset < RoomSlots && clumps.size() - plant_start < MaxVisiblePlants; ++offset) {
            for (const auto& cover : room_cover[(first + offset) % RoomSlots]) {
                for (const auto& plant : cover.plants) {
                    if (clumps.size() - plant_start >= MaxVisiblePlants) break;
                    const auto& root = plant.root;
                    const float dx = root.point.x - camera.eye[0], dy = root.point.y - camera.eye[1],
                                dz = root.point.z - camera.eye[2];
                    const float distance2 = dx*dx + dy*dy + dz*dz;
                    if (distance2 >= PlantDistance * PlantDistance || !visible(root.point)) continue;
                    const float distance = std::sqrt(distance2);
                    const float density = std::min(1.0f, 2000.0f * 2000.0f / std::max(distance2, 1.0f));
                    std::uint32_t seed = std::bit_cast<std::uint32_t>(root.point.x) * 0x9e3779b9u ^
                                         std::bit_cast<std::uint32_t>(root.point.z) * 0x85ebca6bu;
                    const float fade = std::clamp((PlantDistance - distance) / 1200.0f, 0.0f, 1.0f) *
                                       std::clamp((density - random(seed)) * 16.0f, 0.0f, 1.0f);
                    if (root.height * fade < 0.5f) continue;
                    const auto trail=trail_bend(root.point);
                    trampled+=trail.z>0.05f;
                    clumps.push_back({root.point, {-root.height * fade, root.width * fade, root.angle},
                                      plant.slope, root.color, trail});
                    if (!frame_reported) ++detail_counts[static_cast<unsigned>(DetailStyle::Plant)];
                }
            }
        }
        assembly_cache.remember(assembly_key, grass_ready, near_count, plant_start);
    }
    cpu_profile.mark(3);
    if (trampled && !trail_reported) {
        char message[160];
        std::snprintf(message,sizeof(message),"Ground movement trails active: %u nearby clumps, %u retained segments; gradual recovery",trampled,trail_history.active());
        svc_log->info(mod_ctx,message);
        trail_reported=true;
    }
    if (!frame_reported && !clumps.empty() && grass_ready) {
        char message[256];
        std::snprintf(message, sizeof(message),
                      "Ground cover visible: %zu grass roots (%u near, %u distant, %u canopy), view reach %.0f",
                      grass_count, near_count, distant_count, canopy_count, canopy_reach);
        svc_log->info(mod_ctx, message);
        std::snprintf(message, sizeof(message),
                      "Ground detail visible: %u stones, %u leaves, %u twigs, %u plants, %u moss patches",
                      detail_counts[0], detail_counts[1], detail_counts[2], detail_counts[3], detail_counts[4]);
        svc_log->info(mod_ctx, message);
        frame_reported = true;
    }
    if (clumps.empty()) { instance_pool.invalidate(); cpu_profile.finish(0, grass_ready); return; }
    Draw draw{{}, {}, static_cast<unsigned>(clumps.size())};
    draw.immutable = instance_pool.prepare(grass_device.device, std::as_bytes(std::span(clumps)),
        reuse_assembly && !trails_changed);
    CoverUniforms uniforms{};
    uniforms.wind = wind::sample();
    uniforms.actors = terrain_interaction::sample();
    const auto ambient = g_env_light.bg_amb_col[0];
    const auto& source = g_env_light.base_light;
    const bool valid_light = std::isfinite(source.mPosition.x) && std::isfinite(source.mPosition.y) &&
                             std::isfinite(source.mPosition.z);
    const auto channel = [](float value) { return std::clamp(value / 255.0f, 0.0f, 1.0f); };
    uniforms.ambient = {0.42f + 0.58f * channel(ambient.r), 0.42f + 0.58f * channel(ambient.g),
                        0.42f + 0.58f * channel(ambient.b), 0};
    // Outdoor sky supplies a broad fill tint, including blue night skies.
    const auto sky = g_env_light.vrbox_sky_col;
    const float sky_peak = std::max({float(sky.r), float(sky.g), float(sky.b)});
    if (dKy_Outdoor_check() && !g_env_light.hide_vrbox && sky_peak > 0) {
        const float tint[]{sky.r / sky_peak, sky.g / sky_peak, sky.b / sky_peak};
        const float brightness = 0.35f + 0.65f * std::sqrt(channel(sky_peak));
        for (unsigned i = 0; i < 3; ++i)
            uniforms.ambient[i] *= brightness * std::lerp(1.0f, tint[i], 0.38f);
    }
    if (dKy_Outdoor_check() && !g_env_light.hide_vrbox) {
        // The native base light remains white at night. Use the real sun's
        // color and strength outdoors rather than inventing a night sun.
        const auto environment = water_probe::environment();
        std::copy_n(environment.sun_position, 3, uniforms.light_position.begin());
        for (unsigned i = 0; i < 3; ++i)
            uniforms.direct[i] = 0.50f * environment.sun_color[i] * environment.sun_color[3];
    } else if (valid_light) {
        uniforms.light_position = {source.mPosition.x, source.mPosition.y, source.mPosition.z, 1};
        uniforms.direct = {0.16f * channel(source.mColor.r), 0.16f * channel(source.mColor.g),
                           0.16f * channel(source.mColor.b), 0};
    }
    if (dKy_darkworld_check()) {
        uniforms.ambient[0] *= 0.72f;
        uniforms.ambient[1] *= 0.86f;
        uniforms.direct[0] *= 0.72f;
        uniforms.direct[1] *= 0.86f;
    }
    std::copy_n(camera.eye, 3, uniforms.eye.begin());
    uniforms.fog_color = {channel(g_env_light.fog_col.r), channel(g_env_light.fog_col.g),
                          channel(g_env_light.fog_col.b), 0};
    uniforms.fog_range = {g_env_light.mFogNear, std::max(1.0f, g_env_light.mFogFar - g_env_light.mFogNear), 0, 0};
    std::memcpy(uniforms.projection, camera.proj_from_world, sizeof(uniforms.projection));
    if (cpu_profile.active) {
        cpu_profile.current.uploaded_bytes = draw.immutable ? 0 : clumps.size() * sizeof(Clump);
        cpu_profile.current.resident_bytes = instance_pool.bytes();
        cpu_profile.current.immutable_upload = draw.immutable != nullptr;
    }
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &draw.projection) == MOD_OK &&
        (draw.immutable || svc_gfx->push_verts(mod_ctx, clumps.data(), clumps.size() * sizeof(Clump), 4, &draw.instances) == MOD_OK)) {
        immutable_instances::pin(draw.immutable);
        if (svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw)) != MOD_OK)
            immutable_instances::release(draw.immutable);
    }
    cpu_profile.finish(static_cast<unsigned>(clumps.size()), grass_ready);
    cpu_profile.proof(camera, canopy_reach, first, clumps);
}
} // namespace

unsigned airborne_sources(const CameraInfo& camera, std::span<AirborneSource> output) {
    auto* sample = settings::diagnostics_enabled() ? cpu_profile.airborne_target() : nullptr;
    const auto start = sample ? GrassCpuProfile::Clock::now() : GrassCpuProfile::Clock::time_point{};
    unsigned cached_count = 0;
    if (airborne_cache.load(camera, grass_revision, output, cached_count)) {
        cpu_profile.finish_airborne(sample, start, output.first(cached_count), true);
        return cached_count;
    }
    struct Candidate { AirborneSource source; float distance; };
    std::vector<Candidate> candidates;
    auto offer = [&](const Pebble& detail) {
        const auto style = static_cast<DetailStyle>(static_cast<unsigned>(detail.slope.z));
        if (style != DetailStyle::Plant && style != DetailStyle::Leaf) return;
        const auto& p = detail.root.point;
        const float dx = p.x - camera.eye[0], dy = p.y - camera.eye[1], dz = p.z - camera.eye[2];
        const float distance = dx*dx + dy*dy + dz*dz;
        if (distance > 2200.0f * 2200.0f) return;
        const auto* m = camera.proj_from_world;
        const float y = p.y + 140.0f;
        const float w = m[3]*p.x + m[7]*y + m[11]*p.z + m[15];
        const float x = m[0]*p.x + m[4]*y + m[8]*p.z + m[12];
        const float screen_y = m[1]*p.x + m[5]*y + m[9]*p.z + m[13];
        if (w <= 0 || std::abs(x) > w * 1.3f || std::abs(screen_y) > w * 1.3f) return;
        candidates.push_back({{{p.x, p.y + detail.root.height, p.z}, detail.forest}, distance});
    };
    for (const auto& room : room_cover) for (const auto& cover : room) {
        for (const auto& plant : cover.plants) offer(plant);
        for (const auto& detail : cover.pebbles) offer(detail);
    }
    const auto count = std::min(candidates.size(), output.size());
    std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(),
                      [](const auto& a, const auto& b) { return a.distance < b.distance; });
    for (std::size_t i = 0; i < count; ++i) output[i] = candidates[i].source;
    airborne_cache.store(camera, grass_revision, output.size(), output.first(count));
    cpu_profile.finish_airborne(sample, start, output.first(count), false);
    return static_cast<unsigned>(count);
}

void nearby_spray(const float eye[3], std::span<std::array<float, 4>> sources) {
    const auto distance = [&](const waterfall::Fall* fall) {
        const float dx = eye[0] - fall->foot.x, dy = eye[1] - fall->foot.y, dz = eye[2] - fall->foot.z;
        return dx * dx + dy * dy + dz * dz;
    };
    auto falls = loaded_falls();
    std::erase_if(falls, [&](const auto* fall) { return distance(fall) > 4200.0f * 4200.0f; });
    const auto count = std::min(falls.size(), sources.size());
    std::partial_sort(falls.begin(), falls.begin() + count, falls.end(),
                      [&](const auto* a, const auto* b) { return distance(a) < distance(b); });
    for (std::size_t i = 0; i < count; ++i) {
        const auto& fall = *falls[i];
        sources[i] = {fall.foot.x, fall.foot.y, fall.foot.z,
                      std::clamp(fall.width * 0.55f + 130.0f, 210.0f, 1250.0f)};
    }
}

void initialize() {
    if (!svc_gfx || !svc_gfx->push_uniform || !svc_camera || !svc_camera->get_camera) {
        svc_log->warn(mod_ctx, "Ground cover: graphics or camera service unavailable");
        return;
    }
    waterfall::initialize();
    (void)svc_gfx->get_device_info(mod_ctx, &grass_device);
    GfxDrawTypeDesc draw = GFX_DRAW_TYPE_DESC_INIT;
    draw.label = "MidnaFX ground cover";
    draw.draw = encode;
    if (svc_gfx->register_draw_type(mod_ctx, &draw, &draw_type) != MOD_OK)
        svc_log->warn(mod_ctx, "Ground cover: draw registration failed");
}

void update(float elapsed) {
    if (!draw_type) return;
    if (settings::diagnostics_enabled()) cpu_profile.report();
    else cpu_profile.reset();
    static bool was_ground = false, was_water = false, was_atmosphere = false;
    const bool ground = settings::ground_grass_enabled();
    const bool water = settings::enhanced_water_enabled();
    const bool atmosphere = settings::atmosphere_enabled();
    if (ground != was_ground || water != was_water || atmosphere != was_atmosphere) {
        clear_cover();
        was_ground = ground;
        was_water = water;
        was_atmosphere = atmosphere;
    }
    if (!ground || !dComIfGp_getPlayer(0)) trail_history.clear();
    else if (!dComIfGp_isPauseFlag()) trail_history.update(terrain_interaction::sample(),elapsed);
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
    const bool wanted = ground || water || settings::atmosphere_enabled();
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
    instance_pool.clear_after_drain();
    grass_device = GFX_DEVICE_INFO_INIT;
    frame_reported = false;
    visible_tiles = {};
    std::vector<Clump>().swap(clumps);
}
} // namespace midnafx::render::ground_grass
