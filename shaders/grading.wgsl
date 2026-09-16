struct Grading {
    gain_r: f32,
    gain_g: f32,
    gain_b: f32,
    black_point: f32,
    contrast: f32,
    saturation: f32,
    gamma_inverse: f32,
    rolloff: f32,
    detail_strength: f32,
    difference_gain: f32,
    debug_mode: u32,
    split_x: u32,
}

@group(0) @binding(0) var scene: texture_2d<f32>;
@group(0) @binding(1) var<uniform> grading: Grading;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

fn max_component(value: vec3f) -> f32 {
    return max(max(value.r, value.g), value.b);
}

fn apply_grading(input: vec3f) -> vec3f {
    var color = input * vec3f(grading.gain_r, grading.gain_g, grading.gain_b);
    color = max(color - vec3f(grading.black_point), vec3f(0.0)) /
            (1.0 - grading.black_point);
    color = (color - vec3f(0.5)) * grading.contrast + vec3f(0.5);
    let above_knee = max(color - vec3f(0.65), vec3f(0.0));
    color -= grading.rolloff * above_knee * above_knee /
             (vec3f(0.35) + above_knee);
    let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
    color = vec3f(luma) + (color - vec3f(luma)) * grading.saturation;
    return pow(max(color, vec3f(0.0)), vec3f(grading.gamma_inverse));
}

fn apply_detail(center: vec3f, position: vec2i) -> vec3f {
    let last = vec2i(textureDimensions(scene)) - vec2i(1);
    let north = textureLoad(scene, clamp(position + vec2i(0, -1), vec2i(0), last), 0).rgb;
    let east = textureLoad(scene, clamp(position + vec2i(1, 0), vec2i(0), last), 0).rgb;
    let south = textureLoad(scene, clamp(position + vec2i(0, 1), vec2i(0), last), 0).rgb;
    let west = textureLoad(scene, clamp(position + vec2i(-1, 0), vec2i(0), last), 0).rgb;
    let neighbor_average = (north + east + south + west) * 0.25;
    let highpass = center - neighbor_average;
    let local_min = min(center, min(min(north, east), min(south, west)));
    let local_max = max(center, max(max(north, east), max(south, west)));
    let local_contrast = max_component(local_max - local_min);
    let edge_gate = 1.0 - smoothstep(0.12, 0.42, local_contrast);
    let signal_gate = smoothstep(0.005, 0.03, max_component(abs(highpass)));
    let bounded_highpass = clamp(highpass, vec3f(-0.08), vec3f(0.08));
    return clamp(center + grading.detail_strength * edge_gate * signal_gate * bounded_highpass,
                 vec3f(0.0), vec3f(1.0));
}

fn visualize_clipping(processed: vec3f, highlight: bool) -> vec3f {
    if (highlight) {
        let peak = max_component(processed);
        if (peak >= 0.98) { return vec3f(1.0, 0.0, 1.0); }
        if (peak >= 0.90) { return vec3f(1.0, 0.65, 0.0); }
    } else {
        let floor_value = dot(processed, vec3f(0.2126, 0.7152, 0.0722));
        if (floor_value <= 0.02) { return vec3f(0.0, 0.1, 1.0); }
        if (floor_value <= 0.08) { return vec3f(0.0, 0.8, 1.0); }
    }
    return clamp(processed * 0.3, vec3f(0.0), vec3f(1.0));
}

fn debug_output(source: vec4f, processed: vec3f) -> vec4f {
    if (grading.debug_mode == 3u) {
        let luma = dot(processed, vec3f(0.2126, 0.7152, 0.0722));
        return vec4f(vec3f(clamp(luma, 0.0, 1.0)), source.a);
    }
    if (grading.debug_mode == 4u) {
        return vec4f(visualize_clipping(processed, true), source.a);
    }
    if (grading.debug_mode == 5u) {
        return vec4f(visualize_clipping(processed, false), source.a);
    }
    if (grading.debug_mode == 6u) {
        let delta = clamp(grading.difference_gain * abs(processed - source.rgb),
                          vec3f(0.0), vec3f(1.0));
        return vec4f(delta, source.a);
    }
    return vec4f(processed, source.a);
}

@fragment
fn fs_main(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let source = textureLoad(scene, vec2i(position.xy), 0);
    return vec4f(apply_grading(source.rgb), source.a);
}

@fragment
fn fs_detail(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let source = textureLoad(scene, coord, 0);
    return vec4f(apply_grading(apply_detail(source.rgb, coord)), source.a);
}

@fragment
fn fs_debug(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let source = textureLoad(scene, coord, 0);
    if (grading.debug_mode == 2u && u32(coord.x) < grading.split_x) {
        return source;
    }
    return debug_output(source, apply_grading(source.rgb));
}

@fragment
fn fs_debug_detail(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let coord = vec2i(position.xy);
    let source = textureLoad(scene, coord, 0);
    if (grading.debug_mode == 2u && u32(coord.x) < grading.split_x) {
        return source;
    }
    return debug_output(source, apply_grading(apply_detail(source.rgb, coord)));
}
