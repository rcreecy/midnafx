struct AmbientParams {
    view_from_proj: mat4x4f,
    radius: f32,
    strength: f32,
    projection_y: f32,
    background_depth: f32,
    fog_color: vec4f,
    fog_range: vec4f,
}
@group(0) @binding(0) var scene_color: texture_2d<f32>;
@group(0) @binding(1) var scene_depth: texture_2d<f32>;
@group(0) @binding(2) var<uniform> params: AmbientParams;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let triangle = array<vec2f, 3>(vec2f(-1,-1), vec2f(3,-1), vec2f(-1,3));
    return vec4f(triangle[index], 0, 1);
}
fn view_point(pixel: vec2i) -> vec3f {
    let dimensions = vec2i(textureDimensions(scene_depth));
    let p = clamp(pixel, vec2i(0), dimensions - vec2i(1));
    let depth = textureLoad(scene_depth, p, 0).r;
    if (abs(depth - params.background_depth) < 0.000001) { return vec3f(0); }
    let uv = (vec2f(p) + vec2f(0.5)) / vec2f(dimensions);
    let h = params.view_from_proj * vec4f(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1);
    if (abs(h.w) < 0.000001 || any(h != h) || any(abs(h) > vec4f(1e30))) {
        return vec3f(0);
    }
    return h.xyz / h.w;
}
fn atmosphere(color: vec4f, point: vec3f) -> vec4f {
    if (params.fog_color.a <= 0.0) { return color; }
    let span = max(params.fog_range.y - params.fog_range.x, 1.0);
    let distance = max(-point.z - params.fog_range.x, 0.0) / span;
    // Bounded optical depth keeps nearby characters clear and follows the
    // game's current fog range and palette through room/weather transitions.
    let amount = params.fog_color.a * (1.0 - exp(-2.0 * min(distance, 4.0)));
    return vec4f(mix(color.rgb, params.fog_color.rgb, amount), color.a);
}
@fragment
fn fs_ambient(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let pixel = vec2i(position.xy);
    let color = textureLoad(scene_color, pixel, 0);
    let center = view_point(pixel);
    if (center.z >= -0.0001) { return color; }
    if (params.strength <= 0) { return atmosphere(color, center); }

    // Prefer the derivative on the same surface at depth discontinuities.
    let left = view_point(pixel - vec2i(1,0));
    let right = view_point(pixel + vec2i(1,0));
    let up = view_point(pixel - vec2i(0,1));
    let down = view_point(pixel + vec2i(0,1));
    let dx = select(center - left, right - center,
                    abs(right.z - center.z) < abs(center.z - left.z));
    let dy = select(center - up, down - center,
                    abs(down.z - center.z) < abs(center.z - up.z));
    let perpendicular = cross(dx, dy);
    let normal_length = length(perpendicular);
    if (normal_length < 0.000001 || max(length(dx), length(dy)) > params.radius) {
        return atmosphere(color, center);
    }
    var normal = perpendicular / normal_length;
    if (dot(normal, -center) < 0) { normal = -normal; }
    let dimensions = vec2i(textureDimensions(scene_depth));
    let screen_radius = clamp(params.radius * params.projection_y * f32(dimensions.y) /
                               (2 * abs(center.z)), 1.0, 40.0);
    // A fixed symmetric kernel avoids temporal noise and has no history buffers.
    let directions = array<vec2f,8>(vec2f(1,0), vec2f(0.707107,0.707107),
        vec2f(0,1), vec2f(-0.707107,0.707107), vec2f(-1,0),
        vec2f(-0.707107,-0.707107), vec2f(0,-1), vec2f(0.707107,-0.707107));
    var obscurance = 0.0;
    for (var direction = 0u; direction < 8u; direction++) {
        for (var ring = 0u; ring < 2u; ring++) {
            let scale = select(0.4, 0.9, ring == 1u);
            let sample_pixel = pixel + vec2i(round(directions[direction] * screen_radius * scale));
            if (any(sample_pixel < vec2i(0)) || any(sample_pixel >= dimensions)) { continue; }
            let sample = view_point(sample_pixel);
            let delta = sample - center;
            let distance = length(delta);
            if (sample.z >= -0.0001 || distance < 0.01 || distance >= params.radius) { continue; }
            let angle = max(dot(normal, delta / distance) - 0.08, 0.0);
            let falloff = 1.0 - distance * distance / (params.radius * params.radius);
            obscurance += angle * falloff;
        }
    }
    let visibility = 1.0 - clamp(obscurance * params.strength * (3.0 / 16.0), 0.0, 0.5);
    return atmosphere(vec4f(color.rgb * visibility, color.a), center);
}
