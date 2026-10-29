@group(0) @binding(0) var scene_depth: texture_2d<f32>;
@group(0) @binding(1) var field_input: texture_2d<f32>;
@group(0) @binding(2) var field_output: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var<uniform> params: AmbientParams;
@group(0) @binding(4) var scene_color: texture_2d<f32>;
@group(0) @binding(5) var bounce_input: texture_2d<f32>;
@group(0) @binding(6) var bounce_output: texture_storage_2d<rgba16float, write>;

fn contact_shadow(pixel: vec2i, center: vec3f, normal: vec3f) -> f32 {
    let distance_limit = params.shadow_range.x;
    if (params.light_position.w <= 0 || distance_limit <= 0) { return 0; }
    let light_delta = params.light_position.xyz - center;
    let light_distance = length(light_delta);
    if (light_distance < 0.0001 || light_distance > 1e20 || light_distance != light_distance) { return 0; }
    let direction = light_delta / light_distance;
    let facing = smoothstep(0.05, 0.25, dot(normal, direction));
    if (facing <= 0) { return 0; }
    let full_size = vec2i(textureDimensions(scene_depth));
    let footprint = 2 * abs(center.z) / (params.projection_y * f32(full_size.y));
    let bias = max(distance_limit * 0.01, footprint * 1.25);
    // Distant subpixel contacts are unreliable; preserve native shading there.
    if (bias > distance_limit * 0.15) { return 0; }
    if (dot(light_delta, normal) <= bias) { return 0; }
    let start = center + normal * bias;
    // Offset the origin without extending the ray beyond a nearby point light.
    let march_delta = params.light_position.xyz - start;
    let march_distance = length(march_delta);
    if (march_distance < 0.0001) { return 0; }
    let march_direction = march_delta / march_distance;
    let ray_length = min(distance_limit, march_distance);
    let thickness = max(bias * 2, distance_limit * 0.08);
    var amount = 0.0;
    for (var step = 0u; step < 12u; step++) {
        let fraction = (f32(step) + 0.5) / 12;
        let point = start + march_direction * (fraction * ray_length);
        let projected = params.proj_from_view * vec4f(point, 1);
        if (projected.w <= 0.0001 || any(projected != projected) || any(abs(projected) > vec4f(1e30))) { break; }
        let ndc = projected.xyz / projected.w;
        let uv = vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
        if (any(uv <= vec2f(0)) || any(uv >= vec2f(1)) || ndc.z < 0 || ndc.z > 1) { break; }
        let sample_pixel = vec2i(uv * vec2f(full_size));
        if (all(sample_pixel == pixel)) { continue; }
        let sample = view_point(sample_pixel);
        if (sample.z >= -0.0001) { continue; }
        let along_ray = dot(sample - start, march_direction);
        if (along_ray <= 0 || along_ray >= ray_length) { continue; }
        let gap = sample.z - point.z;
        if (gap > 0 && gap < thickness) {
            let confidence = 1 - smoothstep(thickness * 0.5, thickness, gap);
            let boundary = min(min(uv.x, 1-uv.x) * f32(full_size.x),
                               min(uv.y, 1-uv.y) * f32(full_size.y));
            let edge_fade = smoothstep(0, 4, boundary);
            amount = max(amount, confidence * (1 - pow(fraction, 4)) * edge_fade);
        }
    }
    return params.light_position.w * facing * amount;
}

@compute @workgroup_size(8,8)
fn cs_occlusion(@builtin(global_invocation_id) id: vec3u) {
    let size = textureDimensions(field_output);
    if (any(id.xy >= size)) { return; }
    let pixel = min(vec2i(id.xy * 2 + vec2u(1)), vec2i(textureDimensions(scene_depth)) - 1);
    let sample = view_sample(pixel);
    let center = sample.point;
    if (center.z >= -0.0001) {
        textureStore(field_output, vec2i(id.xy), vec4f(1,0,0,0));
        return;
    }
    let normal = surface_normal(pixel, sample);
    if (dot(normal, normal) < 0.5) {
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
    if (params.strength > 0) {
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
    }
    textureStore(field_output, vec2i(id.xy),
        vec4f(1 - clamp(obscurance * 0.25, 0, 1),
              1 + contact_shadow(pixel, center, normal), encode_normal(normal)));
}

fn bounce_visible(center: vec3f, source: vec3f) -> bool {
    let size = vec2i(textureDimensions(scene_depth));
    for (var tap = 1; tap <= 2; tap++) {
        let point = mix(center, source, f32(tap) / 3);
        let h = params.proj_from_view * vec4f(point, 1);
        if (h.w <= 0.0001 || any(h != h) || any(abs(h) > vec4f(1e30))) { return false; }
        let ndc = h.xyz / h.w;
        let uv = vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
        if (any(uv <= vec2f(0)) || any(uv >= vec2f(1))) { return false; }
        let visible = view_point(vec2i(uv * vec2f(size)));
        // A nearer visible surface interrupts the short source/receiver segment.
        if (visible.z < -0.0001 && visible.z - point.z > params.bounce.y * 0.025) { return false; }
    }
    return true;
}

@compute @workgroup_size(8,8)
fn cs_bounce(@builtin(global_invocation_id) id: vec3u) {
    if (params.bounce.x <= 0 || any(id.xy >= textureDimensions(bounce_output))) { return; }
    let field_pixel = vec2i(id.xy);
    let field = textureLoad(field_input, field_pixel, 0);
    if (field.y <= 0) {
        textureStore(bounce_output, field_pixel, vec4f(0));
        return;
    }
    let center = field_point(field_pixel);
    let normal = decode_normal(field.zw);
    let size = vec2i(textureDimensions(scene_depth));
    let last_field = vec2i(textureDimensions(field_input)) - 1;
    let pixel = min(field_pixel * 2 + vec2i(1), size - 1);
    let radius = params.bounce.y;
    let screen_radius = clamp(radius * params.projection_y * f32(size.y) /
                              (2 * abs(center.z)), 1, max(f32(size.y) / 12, 1));
    let directions = array<vec2f,8>(vec2f(1,0), vec2f(0.707107,0.707107),
        vec2f(0,1), vec2f(-0.707107,0.707107), vec2f(-1,0),
        vec2f(-0.707107,-0.707107), vec2f(0,-1), vec2f(0.707107,-0.707107));
    let rings = array<f32,2>(0.35,0.8);
    var incoming = vec3f(0);
    for (var d = 0u; d < 8u; d++) {
        for (var r = 0u; r < 2u; r++) {
            let candidate = pixel + vec2i(round(directions[d] * screen_radius * rings[r]));
            if (any(candidate < vec2i(0)) || any(candidate >= size)) { continue; }
            let p = min(candidate / 2, last_field);
            if (all(p == field_pixel)) { continue; }
            let other = textureLoad(field_input, p, 0);
            if (other.y <= 0) { continue; }
            let source = field_point(p);
            let delta = source - center;
            let distance = length(delta);
            if (distance < 0.01 || distance >= radius) { continue; }
            let direction = delta / distance;
            let receiver_facing = max(dot(normal, direction) - 0.05, 0);
            let source_facing = max(dot(decode_normal(other.zw), -direction) - 0.05, 0);
            let facing = receiver_facing * source_facing;
            if (facing <= 0.0001 || !bounce_visible(center, source)) { continue; }
            let source_pixel = min(p * 2 + vec2i(1), size - 1);
            let color = clamp(textureLoad(scene_color, source_pixel, 0).rgb, vec3f(0), vec3f(1));
            let falloff = 1 - distance * distance / (radius * radius);
            let edge = min(min(f32(candidate.x), f32(size.x - 1 - candidate.x)),
                           min(f32(candidate.y), f32(size.y - 1 - candidate.y)));
            incoming += bounce_linear(color) * facing * falloff * smoothstep(0, 4, edge);
        }
    }
    // Fixed normalization keeps missing/off-screen sources dark rather than
    // amplifying the remaining samples. This is one bounded LDR bounce only.
    textureStore(bounce_output, field_pixel, vec4f(clamp(incoming * 0.25, vec3f(0), vec3f(1)), 1));
}

fn filter_field(pixel: vec2i, direction: vec2i) -> vec4f {
    let center = textureLoad(field_input, pixel, 0);
    if (center.y <= 0) { return center; }
    let normal = decode_normal(center.zw);
    let center_point = field_point(pixel);
    let last = vec2i(textureDimensions(field_input)) - 1;
    let kernel = array<f32,5>(0.0625,0.25,0.375,0.25,0.0625);
    var sum = vec2f(0);
    var bounce_sum = vec3f(0);
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
        sum += sample.xy * weight;
        if (params.bounce.x > 0) { bounce_sum += textureLoad(bounce_input, p, 0).rgb * weight; }
        weights += weight;
    }
    if (params.bounce.x > 0) {
        textureStore(bounce_output, pixel, vec4f(bounce_sum / max(weights, 0.000001), 1));
    }
    return vec4f(select(center.xy, sum / max(weights, 0.000001), weights > 0.000001), center.zw);
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
