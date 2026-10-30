struct AmbientParams {
    view_from_proj: mat4x4f,
    radius: f32,
    strength: f32,
    projection_y: f32,
    background_depth: f32,
    fog_color: vec4f,
    fog_range: vec4f,
    proj_from_view: mat4x4f,
    light_position: vec4f,
    shadow_range: vec4f,
    bounce: vec4f,
    sun_screen: vec4f,
    sun_color: vec4f,
    local: vec4f,
    local_position: array<vec4f,4>,
    local_color: array<vec4f,4>,
    local_distance: array<vec4f,4>,
}
fn lighting_strength() -> f32 { return max(params.bounce.x, params.local.x); }
// Native scene color is an LDR, display-encoded signal, not material albedo.
fn bounce_linear(color: vec3f) -> vec3f {
    return select(color / 12.92, pow((color + 0.055) / 1.055, vec3f(2.4)), color > vec3f(0.04045));
}
fn bounce_display(color: vec3f) -> vec3f {
    let c = clamp(color, vec3f(0), vec3f(1));
    return select(c * 12.92, 1.055 * pow(c, vec3f(1.0 / 2.4)) - 0.055, c > vec3f(0.0031308));
}
struct DepthSample {
    point: vec3f,
    depth: f32,
}
fn view_sample(pixel: vec2i) -> DepthSample {
    let dimensions = vec2i(textureDimensions(scene_depth));
    let p = clamp(pixel, vec2i(0), dimensions - vec2i(1));
    let depth = textureLoad(scene_depth, p, 0).r;
    if (depth != depth || depth < 0 || depth > 1 ||
        abs(depth - params.background_depth) < 0.000001) { return DepthSample(vec3f(0), depth); }
    let uv = (vec2f(p) + vec2f(0.5)) / vec2f(dimensions);
    let h = params.view_from_proj * vec4f(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1);
    if (abs(h.w) < 0.000001 || any(h != h) || any(abs(h) > vec4f(1e30))) {
        return DepthSample(vec3f(0), depth);
    }
    return DepthSample(h.xyz / h.w, depth);
}
fn view_point(pixel: vec2i) -> vec3f { return view_sample(pixel).point; }

fn normal_derivative(pixel: vec2i, axis: vec2i, center: DepthSample) -> vec3f {
    let size = vec2i(textureDimensions(scene_depth));
    let negative_pixel = pixel - axis;
    let positive_pixel = pixel + axis;
    let negative = view_sample(negative_pixel);
    let positive = view_sample(positive_pixel);
    let negative_valid = all(negative_pixel >= vec2i(0)) && all(negative_pixel < size) && negative.point.z < -0.0001;
    let positive_valid = all(positive_pixel >= vec2i(0)) && all(positive_pixel < size) && positive.point.z < -0.0001;
    if (!negative_valid && !positive_valid) { return vec3f(0); }
    var forward = !negative_valid;
    if (negative_valid && positive_valid) {
        forward = abs(positive.point.z-center.point.z) < abs(center.point.z-negative.point.z);
        let negative_far_pixel = pixel - axis * 2;
        let positive_far_pixel = pixel + axis * 2;
        let negative_far = view_sample(negative_far_pixel);
        let positive_far = view_sample(positive_far_pixel);
        if (all(negative_far_pixel >= vec2i(0)) && all(negative_far_pixel < size) &&
            all(positive_far_pixel >= vec2i(0)) && all(positive_far_pixel < size) &&
            negative_far.point.z < -0.0001 && positive_far.point.z < -0.0001) {
            // Device depth is affine over a projected plane, including reversed Z.
            // Prefer the side whose two samples extrapolate back onto the center.
            let negative_error = abs(2 * negative.depth - negative_far.depth - center.depth);
            let positive_error = abs(2 * positive.depth - positive_far.depth - center.depth);
            // Near ties retain the nearest-depth rule instead of following noise.
            if (abs(positive_error-negative_error) > 0.0000002) {
                forward = positive_error < negative_error;
            }
        }
    }
    return select(center.point-negative.point, positive.point-center.point, forward);
}
fn surface_normal(pixel: vec2i, center: DepthSample) -> vec3f {
    let dx = normal_derivative(pixel, vec2i(1,0), center);
    let dy = normal_derivative(pixel, vec2i(0,1), center);
    let perpendicular = cross(dx, dy);
    let magnitude = length(perpendicular);
    if (magnitude < 0.000001 || max(length(dx), length(dy)) > params.radius) { return vec3f(0); }
    let normal = perpendicular / magnitude;
    return select(normal, -normal, dot(normal, -center.point) < 0);
}
fn encode_normal(normal: vec3f) -> vec2f {
    let n = normal / max(abs(normal.x) + abs(normal.y) + abs(normal.z), 0.000001);
    var encoded = n.xy;
    if (n.z < 0) {
        encoded = (vec2f(1) - abs(encoded.yx)) * select(vec2f(-1), vec2f(1), encoded >= vec2f(0));
    }
    return encoded * 0.5 + 0.5;
}
fn decode_normal(encoded: vec2f) -> vec3f {
    let p = encoded * 2 - 1;
    var n = vec3f(p, 1 - abs(p.x) - abs(p.y));
    let fold = clamp(-n.z, 0, 1);
    n.x += select(fold, -fold, n.x >= 0);
    n.y += select(fold, -fold, n.y >= 0);
    return normalize(n);
}
// Keep full-precision native depth; half-float distance would lose contact detail
// far from the camera. Field channels carry AO visibility, validity plus contact
// shadow amount (zero invalid, one plus shadow valid), and octahedral normal.
fn field_point(pixel: vec2i) -> vec3f {
    let last = vec2i(textureDimensions(scene_depth)) - 1;
    return view_point(min(pixel * 2 + vec2i(1), last));
}
