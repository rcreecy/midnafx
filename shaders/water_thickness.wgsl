struct WaterThicknessDiagnostic {
    view_from_proj: mat4x4f,
    max_thickness: f32,
    background_depth: f32,
    padding: vec2f,
}

@group(0) @binding(0) var scene_depth: texture_2d<f32>;
@group(0) @binding(1) var surface_depth: texture_2d<f32>;
@group(0) @binding(2) var surface_mask: texture_2d<f32>;
@group(0) @binding(3) var<uniform> params: WaterThicknessDiagnostic;

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
