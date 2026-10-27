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
}
fn view_point(pixel: vec2i) -> vec3f {
    let dimensions = vec2i(textureDimensions(scene_depth));
    let p = clamp(pixel, vec2i(0), dimensions - vec2i(1));
    let depth = textureLoad(scene_depth, p, 0).r;
    if (abs(depth - params.background_depth) < 0.000001) { return vec3f(0); }
    let uv = (vec2f(p) + vec2f(0.5)) / vec2f(dimensions);
    let h = params.view_from_proj * vec4f(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1);
    if (abs(h.w) < 0.000001 || any(h != h) || any(abs(h) > vec4f(1e30))) { return vec3f(0); }
    return h.xyz / h.w;
}
fn surface_normal(pixel: vec2i, center: vec3f) -> vec3f {
    let left = view_point(pixel - vec2i(1,0));
    let right = view_point(pixel + vec2i(1,0));
    let up = view_point(pixel - vec2i(0,1));
    let down = view_point(pixel + vec2i(0,1));
    let dx = select(center - left, right - center, abs(right.z-center.z) < abs(center.z-left.z));
    let dy = select(center - up, down - center, abs(down.z-center.z) < abs(center.z-up.z));
    let perpendicular = cross(dx, dy);
    let magnitude = length(perpendicular);
    if (magnitude < 0.000001 || max(length(dx), length(dy)) > params.radius) { return vec3f(0); }
    let normal = perpendicular / magnitude;
    return select(normal, -normal, dot(normal, -center) < 0);
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
