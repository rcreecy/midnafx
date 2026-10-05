struct WaterParameters {
    view_from_proj: mat4x4f,
    world_from_proj: mat4x4f,
    view_from_world: mat4x4f,
    max_thickness: f32,
    background_depth: f32,
    absorption_strength: f32,
    animation_time: f32,
    shallow_tint: vec4f,
    deep_tint: vec4f,
    wave_strength: f32,
    wave_scale: f32,
    wave_speed: f32,
    refraction_strength: f32,
    reflection_tint: vec4f,
    reflection_strength: f32,
    reflection_padding_0: f32,
    reflection_padding_1: f32,
    reflection_padding_2: f32,
    shoreline_strength: f32,
    shoreline_padding_0: f32,
    shoreline_padding_1: f32,
    shoreline_padding_2: f32,
    gain_r: f32,
    gain_g: f32,
    gain_b: f32,
    black_point: f32,
    contrast: f32,
    saturation: f32,
    gamma_inverse: f32,
    rolloff: f32,
    detail_strength: f32,
    difference_gain: f32,
    debug_mode: u32,
    split_x: u32,
}

@group(0) @binding(0) var scene_color: texture_2d<f32>;
@group(0) @binding(1) var scene_depth: texture_2d<f32>;
@group(0) @binding(2) var surface_depth: texture_2d<f32>;
@group(0) @binding(3) var surface_mask: texture_2d<f32>;
@group(0) @binding(4) var<uniform> params: WaterParameters;
@group(0) @binding(5) var scene_behind_water: texture_2d<f32>;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

fn view_distance(coord: vec2i, depth: f32, dimensions: vec2f) -> f32 {
    let uv = (vec2f(coord) + vec2f(0.5)) / dimensions;
    let ndc = vec3f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let view4 = params.view_from_proj * vec4f(ndc, 1.0);
    if (abs(view4.w) <= 0.000001 || any(view4 != view4)) {
        return -1.0;
    }
    let distance = length(view4.xyz / view4.w);
    return select(distance, -1.0, distance != distance || distance > 3.402823e38);
}

fn world_position(coord: vec2i, depth: f32, dimensions: vec2f) -> vec3f {
    let uv = (vec2f(coord) + vec2f(0.5)) / dimensions;
    let ndc = vec3f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let world4 = params.world_from_proj * vec4f(ndc, 1.0);
    return world4.xyz / world4.w;
}

fn water_surface_normal(coord: vec2i, depth: f32, dimensions: vec2f) -> vec3f {
    let world = world_position(coord, depth, dimensions);
    if (any(world != world) || any(abs(world) > vec3f(3.402823e38))) {
        return vec3f(0.0, 1.0, 0.0);
    }
    let time = params.animation_time * params.wave_speed;
    let large = world.xz * params.wave_scale + vec2f(time, time * 0.37);
    let ripple = world.xz * (params.wave_scale * 2.35) + vec2f(-time * 0.71, time * 0.53);
    let large_slope = vec2f(sin(large.x + cos(large.y)), cos(large.y + sin(large.x)));
    let ripple_slope = vec2f(cos(ripple.x - sin(ripple.y)), sin(ripple.y + cos(ripple.x)));
    let slope = (large_slope * 0.65 + ripple_slope * 0.35) * params.wave_strength * 0.28;
    return normalize(vec3f(-slope.x, 1.0, -slope.y));
}

fn refracted_source(coord: vec2i, water_depth: f32, normalized_depth: f32,
                    dimensions: vec2f, native_source: vec3f, normal: vec3f) -> vec3f {
    if (params.refraction_strength <= 0.0) {
        return native_source;
    }
    let view_normal = normalize((params.view_from_world * vec4f(normal, 0.0)).xyz);
    let depth_scale = smoothstep(0.0, 0.35, normalized_depth);
    let offset = view_normal.xy * params.refraction_strength * mix(1.0, 8.0, depth_scale);
    let last = vec2i(dimensions) - vec2i(1);
    let sample_coord = clamp(coord + vec2i(round(offset)), vec2i(0), last);
    let sample_depth = textureLoad(scene_depth, sample_coord, 0).r;
    if (abs(sample_depth - params.background_depth) <= 0.000001) {
        return native_source;
    }
    let water_distance = view_distance(coord, water_depth, dimensions);
    let sample_distance = view_distance(sample_coord, sample_depth, dimensions);
    if (sample_distance <= water_distance) {
        return native_source;
    }
    let refracted = textureLoad(scene_behind_water, sample_coord, 0).rgb;
    let blend = params.refraction_strength * mix(0.08, 0.35, depth_scale);
    return mix(native_source, refracted, blend);
}

fn apply_fresnel_reflection(coord: vec2i, water_depth: f32, dimensions: vec2f,
                            transmitted: vec3f, normal: vec3f) -> vec3f {
    if (params.reflection_strength <= 0.0) {
        return transmitted;
    }
    let world = world_position(coord, water_depth, dimensions);
    let view_position = (params.view_from_world * vec4f(world, 1.0)).xyz;
    let raw_view_normal = (params.view_from_world * vec4f(normal, 0.0)).xyz;
    let view_length = length(view_position);
    let normal_length = length(raw_view_normal);
    if (view_length <= 0.000001 || normal_length <= 0.000001 ||
        view_length != view_length || normal_length != normal_length) {
        return transmitted;
    }
    let view_normal = raw_view_normal / normal_length;
    let view_direction = -view_position / view_length;
    let facing = clamp(dot(view_direction, view_normal), 0.0, 1.0);
    let fresnel = 0.02 + 0.98 * pow(1.0 - facing, 5.0);
    let weight = clamp(fresnel * params.reflection_strength, 0.0, 0.45);
    return mix(transmitted, params.reflection_tint.rgb, weight);
}

fn apply_shoreline_treatment(color: vec3f, normalized_depth: f32,
                             normal: vec3f) -> vec3f {
    if (params.shoreline_strength <= 0.0) {
        return color;
    }
    let shallow = 1.0 - smoothstep(0.0, 0.08, normalized_depth);
    let ripple = 0.75 + 0.25 * clamp(length(normal.xz) * 8.0, 0.0, 1.0);
    let weight = shallow * ripple * params.shoreline_strength * 0.16;
    let highlight = min(params.shallow_tint.rgb * 1.18, vec3f(1.0));
    return mix(color, highlight, clamp(weight, 0.0, 0.16));
}

fn max_component(value: vec3f) -> f32 {
    return max(max(value.r, value.g), value.b);
}

fn apply_detail(center: vec3f, position: vec2i) -> vec3f {
    let last = vec2i(textureDimensions(scene_color)) - vec2i(1);
    let north = textureLoad(scene_color, clamp(position + vec2i(0, -1), vec2i(0), last), 0).rgb;
    let east = textureLoad(scene_color, clamp(position + vec2i(1, 0), vec2i(0), last), 0).rgb;
    let south = textureLoad(scene_color, clamp(position + vec2i(0, 1), vec2i(0), last), 0).rgb;
    let west = textureLoad(scene_color, clamp(position + vec2i(-1, 0), vec2i(0), last), 0).rgb;
    let neighbor_average = (north + east + south + west) * 0.25;
    let highpass = center - neighbor_average;
    let local_min = min(center, min(min(north, east), min(south, west)));
    let local_max = max(center, max(max(north, east), max(south, west)));
    let local_contrast = max_component(local_max - local_min);
    let edge_gate = 1.0 - smoothstep(0.12, 0.42, local_contrast);
    let signal_gate = smoothstep(0.005, 0.03, max_component(abs(highpass)));
    let bounded_highpass = clamp(highpass, vec3f(-0.08), vec3f(0.08));
    return clamp(center + params.detail_strength * edge_gate * signal_gate * bounded_highpass,
                 vec3f(0.0), vec3f(1.0));
}

fn apply_grading(input: vec3f) -> vec3f {
    var color = input * vec3f(params.gain_r, params.gain_g, params.gain_b);
    color = max(color - vec3f(params.black_point), vec3f(0.0)) /
            (1.0 - params.black_point);
    color = (color - vec3f(0.5)) * params.contrast + vec3f(0.5);
    let above_knee = max(color - vec3f(0.65), vec3f(0.0));
    color -= params.rolloff * above_knee * above_knee / (vec3f(0.35) + above_knee);
    let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
    color = vec3f(luma) + (color - vec3f(luma)) * params.saturation;
    return pow(max(color, vec3f(0.0)), vec3f(params.gamma_inverse));
}

fn apply_absorption(coord: vec2i, source: vec3f) -> vec3f {
    if (textureLoad(surface_mask, coord, 0).a < 0.5) {
        return source;
    }
    let water_depth = textureLoad(surface_depth, coord, 0).r;
    let opaque_depth = textureLoad(scene_depth, coord, 0).r;
    if (abs(water_depth - params.background_depth) <= 0.000001 ||
        abs(opaque_depth - params.background_depth) <= 0.000001) {
        return source;
    }
    let dimensions = vec2f(textureDimensions(scene_depth));
    let water_distance = view_distance(coord, water_depth, dimensions);
    let opaque_distance = view_distance(coord, opaque_depth, dimensions);
    if (water_distance < 0.0 || opaque_distance <= water_distance) {
        return source;
    }
    let normalized = clamp((opaque_distance - water_distance) / params.max_thickness, 0.0, 1.0);
    let transmission = exp(-params.absorption_strength * normalized);
    let scatter = mix(params.shallow_tint.rgb, params.deep_tint.rgb,
                      smoothstep(0.0, 1.0, normalized));
    let surface_normal = water_surface_normal(coord, water_depth, dimensions);
    let wave_light = 1.0 + dot(surface_normal.xz, normalize(vec2f(0.8, 0.6))) * 0.10;
    // Some TP water paths have no opaque color behind the surface at the
    // pre-water boundary. Keep those pixels visibly water-colored instead of
    // turning the whole surface black until a submerged-color source exists.
    let source_luma = dot(source, vec3f(0.2126, 0.7152, 0.0722));
    let stable_source = select(source, max(source, params.shallow_tint.rgb * 0.35), source_luma < 0.01);
    let transmitted_source = refracted_source(coord, water_depth, normalized, dimensions,
                                               stable_source, surface_normal);
    let absorbed = (transmitted_source * transmission + scatter * (1.0 - transmission)) * wave_light;
    let shoreline = apply_shoreline_treatment(absorbed, normalized, surface_normal);
    return apply_fresnel_reflection(coord, water_depth, dimensions, shoreline, surface_normal);
}

@fragment
fn fs_thickness(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    if (textureLoad(surface_mask, coord, 0).a < 0.5) {
        return vec4f(0.0, 0.0, 0.0, 1.0);
    }
    let water_depth = textureLoad(surface_depth, coord, 0).r;
    let opaque_depth = textureLoad(scene_depth, coord, 0).r;
    if (abs(water_depth - params.background_depth) <= 0.000001) {
        return vec4f(1.0, 0.0, 1.0, 1.0);
    }
    let dimensions = vec2f(textureDimensions(scene_depth));
    let water_distance = view_distance(coord, water_depth, dimensions);
    if (water_distance < 0.0) {
        return vec4f(1.0, 0.0, 0.0, 1.0);
    }
    if (abs(opaque_depth - params.background_depth) <= 0.000001) {
        return vec4f(0.0, 0.05, 0.2, 1.0);
    }
    let opaque_distance = view_distance(coord, opaque_depth, dimensions);
    if (opaque_distance < 0.0) {
        return vec4f(1.0, 0.0, 0.0, 1.0);
    }
    if (opaque_distance <= water_distance) {
        // The classified water plane may continue behind banks, actors, and
        // other opaque scene geometry. Those pixels are not visible water.
        return vec4f(0.0, 0.0, 0.0, 1.0);
    }
    let normalized = clamp((opaque_distance - water_distance) / params.max_thickness, 0.0, 1.0);
    return vec4f(vec3f(normalized), 1.0);
}

@fragment
fn fs_absorption(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let source = textureLoad(scene_color, coord, 0);
    return vec4f(apply_grading(apply_absorption(coord, source.rgb)), source.a);
}

@fragment
fn fs_absorption_detail(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let source = textureLoad(scene_color, coord, 0);
    let detailed = apply_detail(source.rgb, coord);
    return vec4f(apply_grading(apply_absorption(coord, detailed)), source.a);
}
