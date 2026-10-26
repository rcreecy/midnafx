@group(0) @binding(10) var exposure_history: texture_2d<f32>;

fn apply_exposure(input: vec3f) -> vec3f {
    let gain = exp2(textureLoad(exposure_history, vec2i(0), 0).x);
    let peak = max(max(input.r, input.g), input.b);
    // Preserve hue, black and bright endpoints when opening shadows.
    return input * (gain / (1.0 + max(gain - 1.0, 0.0) * peak));
}
