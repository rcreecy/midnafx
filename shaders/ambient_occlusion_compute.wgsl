@group(0) @binding(0) var scene_depth: texture_2d<f32>;
@group(0) @binding(1) var field_input: texture_2d<f32>;
@group(0) @binding(2) var field_output: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var<uniform> params: AmbientParams;

@compute @workgroup_size(8,8)
fn cs_occlusion(@builtin(global_invocation_id) id: vec3u) {
    let size = textureDimensions(field_output);
    if (any(id.xy >= size)) { return; }
    let pixel = min(vec2i(id.xy * 2 + vec2u(1)), vec2i(textureDimensions(scene_depth)) - 1);
    let center = view_point(pixel);
    let normal = surface_normal(pixel, center);
    if (center.z >= -0.0001 || dot(normal, normal) < 0.5) {
        textureStore(field_output, vec2i(id.xy), vec4f(1,0,0,0));
        return;
    }
    let full_size = vec2i(textureDimensions(scene_depth));
    let screen_radius = clamp(params.radius * params.projection_y * f32(full_size.y) /
                               (2 * abs(center.z)), 1, max(f32(full_size.y) / 18, 1));
    let directions = array<vec2f,8>(vec2f(1,0), vec2f(0.707107,0.707107),
        vec2f(0,1), vec2f(-0.707107,0.707107), vec2f(-1,0),
        vec2f(-0.707107,-0.707107), vec2f(0,-1), vec2f(0.707107,-0.707107));
    let rings = array<f32,4>(0.15,0.35,0.6,0.9);
    var obscurance = 0.0;
    for (var direction = 0u; direction < 8u; direction++) {
        var horizon = 0.0;
        for (var ring = 0u; ring < 4u; ring++) {
            let p = pixel + vec2i(round(directions[direction] * screen_radius * rings[ring]));
            if (any(p < vec2i(0)) || any(p >= full_size)) { continue; }
            let sample = view_point(p);
            let delta = sample - center;
            let distance = length(delta);
            if (sample.z >= -0.0001 || distance < 0.01 || distance >= params.radius) { continue; }
            let angle = max(dot(normal, delta / distance) - 0.08, 0);
            let falloff = 1 - distance * distance / (params.radius * params.radius);
            horizon = max(horizon, angle * falloff);
        }
        obscurance += horizon;
    }
    textureStore(field_output, vec2i(id.xy),
        vec4f(1 - clamp(obscurance * 0.25, 0, 1), 1, encode_normal(normal)));
}

fn filter_field(pixel: vec2i, direction: vec2i) -> vec4f {
    let center = textureLoad(field_input, pixel, 0);
    if (center.y <= 0) { return center; }
    let normal = decode_normal(center.zw);
    let center_point = field_point(pixel);
    let last = vec2i(textureDimensions(field_input)) - 1;
    let kernel = array<f32,5>(0.0625,0.25,0.375,0.25,0.0625);
    var sum = 0.0;
    var weights = 0.0;
    for (var tap = -2; tap <= 2; tap++) {
        let p = clamp(pixel + direction * tap, vec2i(0), last);
        let sample = textureLoad(field_input, p, 0);
        if (sample.y <= 0) { continue; }
        let other_normal = decode_normal(sample.zw);
        let delta = field_point(p) - center_point;
        let plane_distance = max(abs(dot(delta, normal)), abs(dot(delta, other_normal)));
        let edge = 1 - smoothstep(params.radius * 0.015, params.radius * 0.12, plane_distance);
        let weight = kernel[u32(tap + 2)] * edge * pow(max(dot(normal, other_normal), 0), 16);
        sum += sample.x * weight;
        weights += weight;
    }
    return vec4f(select(center.x, sum / max(weights, 0.000001), weights > 0.000001), center.yzw);
}

@compute @workgroup_size(8,8)
fn cs_filter_horizontal(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    textureStore(field_output, vec2i(id.xy), filter_field(vec2i(id.xy), vec2i(1,0)));
}
@compute @workgroup_size(8,8)
fn cs_filter_vertical(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    textureStore(field_output, vec2i(id.xy), filter_field(vec2i(id.xy), vec2i(0,1)));
}
