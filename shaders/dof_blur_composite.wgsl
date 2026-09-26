struct DofBlurParams {
    view_from_proj: mat4x4f,
    full_size: vec2f,
    half_size: vec2f,
    near_plane: f32,
    far_plane: f32,
    focus_distance: f32,
    focus_range: f32,
    background_depth: f32,
    blur_radius: f32,
    padding: vec2f,
}

@group(0) @binding(0) var scene_color: texture_2d<f32>;
@group(0) @binding(1) var scene_depth: texture_2d<f32>;
@group(0) @binding(2) var near_blur: texture_2d<f32>;
@group(0) @binding(3) var far_blur: texture_2d<f32>;
@group(0) @binding(4) var<uniform> params: DofBlurParams;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

fn finite_vector(value: vec4f) -> bool {
    return all(value == value) && all(abs(value) <= vec4f(3.402823e38));
}

fn signed_coc(coord: vec2i, depth: f32) -> f32 {
    if (abs(depth - params.background_depth) <= 0.000001) {
        return 1.0;
    }
    let uv = (vec2f(coord) + vec2f(0.5)) / params.full_size;
    let ndc = vec3f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let view4 = params.view_from_proj * vec4f(ndc, 1.0);
    if (abs(view4.w) <= 0.000001 || !finite_vector(view4)) {
        return 0.0;
    }
    let distance = abs(view4.z / view4.w);
    if (!(distance == distance) || distance < params.near_plane * 0.5 ||
        distance > params.far_plane * 1.01) {
        return 0.0;
    }
    return clamp((distance - params.focus_distance) / params.focus_range, -1.0, 1.0);
}

fn half_sample(texture: texture_2d<f32>, full_position: vec2f) -> vec4f {
    let sample_position = full_position * 0.5 - vec2f(0.5);
    let base = vec2i(floor(sample_position));
    let fraction = fract(sample_position);
    let dimensions = vec2i(params.half_size);
    let maximum = dimensions - vec2i(1);
    let p00 = clamp(base, vec2i(0), maximum);
    let p10 = clamp(base + vec2i(1, 0), vec2i(0), maximum);
    let p01 = clamp(base + vec2i(0, 1), vec2i(0), maximum);
    let p11 = clamp(base + vec2i(1, 1), vec2i(0), maximum);
    let top = mix(textureLoad(texture, p00, 0), textureLoad(texture, p10, 0), fraction.x);
    let bottom = mix(textureLoad(texture, p01, 0), textureLoad(texture, p11, 0), fraction.x);
    return mix(top, bottom, fraction.y);
}

@fragment
fn fs_composite(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = min(vec2i(position.xy), vec2i(params.full_size) - vec2i(1));
    let source = textureLoad(scene_color, coord, 0);
    let coc = signed_coc(coord, textureLoad(scene_depth, coord, 0).r);
    let near_value = half_sample(near_blur, position.xy);
    let far_value = half_sample(far_blur, position.xy);
    let near_color = near_value.rgb / max(near_value.a, 0.0001);
    let far_color = far_value.rgb / max(far_value.a, 0.0001);
    let far_amount = clamp(max(coc, 0.0) * far_value.a, 0.0, 1.0);
    let near_amount = clamp(near_value.a * 1.5, 0.0, 1.0);
    let far_composite = mix(source.rgb, far_color, far_amount);
    return vec4f(mix(far_composite, near_color, near_amount), source.a);
}
