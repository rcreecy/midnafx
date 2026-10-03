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

@group(0) @binding(0) var input_near: texture_2d<f32>;
@group(0) @binding(1) var input_far: texture_2d<f32>;
@group(0) @binding(2) var output_near: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var output_far: texture_storage_2d<rgba16float, write>;
@group(0) @binding(4) var<uniform> params: DofBlurParams;

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

@compute @workgroup_size(8, 8)
fn cs_downsample(@builtin(global_invocation_id) id: vec3u) {
    let half_coord = vec2i(id.xy);
    let half_dimensions = vec2i(params.half_size);
    if (any(half_coord >= half_dimensions)) {
        return;
    }
    let full_dimensions = vec2i(params.full_size);
    let base = half_coord * 2;
    var near_sum = vec4f(0.0);
    var far_sum = vec4f(0.0);
    for (var y = 0; y < 2; y++) {
        for (var x = 0; x < 2; x++) {
            let coord = min(base + vec2i(x, y), full_dimensions - vec2i(1));
            let color = textureLoad(input_near, coord, 0).rgb;
            let coc = signed_coc(coord, textureLoad(input_far, coord, 0).r);
            let near_weight = max(-coc, 0.0);
            let far_weight = max(coc, 0.0);
            near_sum += vec4f(color * near_weight, near_weight);
            far_sum += vec4f(color * far_weight, far_weight);
        }
    }
    textureStore(output_near, half_coord, near_sum * 0.25);
    textureStore(output_far, half_coord, far_sum * 0.25);
}

const gaussian = array<f32, 5>(0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

fn blur(coord: vec2i, horizontal: bool) {
    let dimensions = vec2i(params.half_size);
    var near_sum = textureLoad(input_near, coord, 0) * gaussian[0];
    var far_sum = textureLoad(input_far, coord, 0) * gaussian[0];
    for (var tap = 1; tap <= 4; tap++) {
        // Convert requested full-resolution radius into each Gaussian tap's
        // half-resolution offset. Per-tap rounding preserves useful steps
        // across the full 2-12 pixel control range.
        let tap_offset = max(i32(round(f32(tap) * params.blur_radius / 8.0)), 1);
        let delta = select(vec2i(0, tap_offset), vec2i(tap_offset, 0), horizontal);
        let low = clamp(coord - delta, vec2i(0), dimensions - vec2i(1));
        let high = clamp(coord + delta, vec2i(0), dimensions - vec2i(1));
        let weight = gaussian[tap];
        near_sum += (textureLoad(input_near, low, 0) + textureLoad(input_near, high, 0)) * weight;
        far_sum += (textureLoad(input_far, low, 0) + textureLoad(input_far, high, 0)) * weight;
    }
    textureStore(output_near, coord, near_sum);
    textureStore(output_far, coord, far_sum);
}

@compute @workgroup_size(8, 8)
fn cs_blur_horizontal(@builtin(global_invocation_id) id: vec3u) {
    let coord = vec2i(id.xy);
    if (any(coord >= vec2i(params.half_size))) {
        return;
    }
    blur(coord, true);
}

@compute @workgroup_size(8, 8)
fn cs_blur_vertical(@builtin(global_invocation_id) id: vec3u) {
    let coord = vec2i(id.xy);
    if (any(coord >= vec2i(params.half_size))) {
        return;
    }
    blur(coord, false);
}
