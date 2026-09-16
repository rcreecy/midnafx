struct Grading {
    gain_r: f32,
    gain_g: f32,
    gain_b: f32,
    black_point: f32,
    contrast: f32,
    saturation: f32,
    gamma_inverse: f32,
    rolloff: f32,
}

@group(0) @binding(0) var scene: texture_2d<f32>;
@group(0) @binding(1) var<uniform> grading: Grading;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

@fragment
fn fs_main(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let source = textureLoad(scene, vec2i(position.xy), 0);
    var color = source.rgb * vec3f(grading.gain_r, grading.gain_g, grading.gain_b);
    color = max(color - vec3f(grading.black_point), vec3f(0.0)) /
            (1.0 - grading.black_point);
    color = (color - vec3f(0.5)) * grading.contrast + vec3f(0.5);
    let above_knee = max(color - vec3f(0.65), vec3f(0.0));
    color -= grading.rolloff * above_knee * above_knee /
             (vec3f(0.35) + above_knee);
    let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
    color = vec3f(luma) + (color - vec3f(luma)) * grading.saturation;
    color = pow(max(color, vec3f(0.0)), vec3f(grading.gamma_inverse));
    return vec4f(color, source.a);
}
