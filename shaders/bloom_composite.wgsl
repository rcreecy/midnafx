@group(0) @binding(11) var bloom_field: texture_2d<f32>;
@group(0) @binding(12) var bloom_sampler: sampler;

fn apply_bloom(input: vec3f, pixel: vec2i, size: vec2u) -> vec3f {
    let uv = (vec2f(pixel) + 0.5) / vec2f(size);
    let glow = textureSampleLevel(bloom_field, bloom_sampler, uv, 0).rgb;
    // Screen-like composition preserves white and cannot clip LDR highlights.
    return input + glow * (vec3f(1) - clamp(input, vec3f(0), vec3f(1)));
}
