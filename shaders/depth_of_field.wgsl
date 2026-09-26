struct DofDiagnostic {
    view_from_proj: mat4x4f,
    near_plane: f32,
    far_plane: f32,
    focus_distance: f32,
    focus_range: f32,
    background_depth: f32,
    padding0: f32,
    padding1: f32,
    padding2: f32,
}

@group(0) @binding(0) var scene_depth: texture_2d<f32>;
@group(0) @binding(1) var<uniform> params: DofDiagnostic;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

fn finite_scalar(value: f32) -> bool {
    return value == value && abs(value) <= 3.402823e38;
}

fn finite_vector(value: vec4f) -> bool {
    return all(value == value) && all(abs(value) <= vec4f(3.402823e38));
}

@fragment
fn fs_coc(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let dimensions = vec2f(textureDimensions(scene_depth));
    let depth = textureLoad(scene_depth, coord, 0).r;
    if (abs(depth - params.background_depth) <= 0.000001) {
        return vec4f(0.0, 0.02, 0.08, 1.0);
    }

    let uv = position.xy / dimensions;
    let ndc = vec3f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let view4 = params.view_from_proj * vec4f(ndc, 1.0);
    if (abs(view4.w) <= 0.000001 || !finite_vector(view4)) {
        return vec4f(1.0, 0.0, 1.0, 1.0);
    }

    let axial_distance = abs(view4.z / view4.w);
    if (!finite_scalar(axial_distance) || axial_distance < params.near_plane * 0.5 ||
        axial_distance > params.far_plane * 1.01) {
        return vec4f(1.0, 0.0, 1.0, 1.0);
    }

    let signed_coc = clamp(
        (axial_distance - params.focus_distance) / params.focus_range, -1.0, 1.0);
    let near_color = vec3f(0.0, 0.75, 1.0) * max(-signed_coc, 0.0);
    let far_color = vec3f(1.0, 0.35, 0.0) * max(signed_coc, 0.0);
    return vec4f(near_color + far_color, 1.0);
}
