struct BloomControls {
    strength: f32,
    blur_scale: f32,
    padding: vec2f,
}
@group(0) @binding(0) var scene: texture_2d<f32>;
@group(0) @binding(1) var field_input: texture_2d<f32>;
@group(0) @binding(2) var field_output: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var linear_sampler: sampler;
@group(0) @binding(4) var<uniform> controls: BloomControls;
@group(0) @binding(5) var fine_input: texture_2d<f32>;

@compute @workgroup_size(8,8)
fn cs_extract(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    let size = textureDimensions(scene);
    var sum = vec3f(0);
    var count = 0.0;
    // Threshold before averaging: small bright features survive downsampling.
    // Visit every source pixel once, without repeating odd-size edge pixels.
    for (var y = 0u; y < 4u; y++) {
        for (var x = 0u; x < 4u; x++) {
            let p = id.xy * 4u + vec2u(x,y);
            if (any(p >= size)) { continue; }
            let color = textureLoad(scene, vec2i(p), 0).rgb;
            let peak = max(max(color.r, color.g), color.b);
            let bright = smoothstep(0.65, 0.95, peak) * max(peak - 0.65, 0) / max(peak, 0.0001);
            sum += color * bright;
            count += 1;
        }
    }
    textureStore(field_output, vec2i(id.xy), vec4f(sum * controls.strength / max(count, 1), 0));
}

fn blur(pixel: vec2u, direction: vec2f) -> vec4f {
    let size = vec2f(textureDimensions(field_input));
    let uv = (vec2f(pixel) + 0.5) / size;
    let step = direction * controls.blur_scale / size;
    let weights = array<f32,5>(0.227027027,0.194594595,0.121621622,0.054054054,0.016216216);
    var color = textureSampleLevel(field_input, linear_sampler, uv, 0).rgb * weights[0];
    for (var tap = 1u; tap < 5u; tap++) {
        let offset = step * f32(tap);
        color += (textureSampleLevel(field_input, linear_sampler, uv + offset, 0).rgb +
                  textureSampleLevel(field_input, linear_sampler, uv - offset, 0).rgb) * weights[tap];
    }
    return vec4f(color, 0);
}

@compute @workgroup_size(8,8)
fn cs_blur_horizontal(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    textureStore(field_output, vec2i(id.xy), blur(id.xy, vec2f(1,0)));
}
@compute @workgroup_size(8,8)
fn cs_blur_vertical(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    textureStore(field_output, vec2i(id.xy), blur(id.xy, vec2f(0,1)));
}

// Build coarser fields from the filtered core. Average valid texels only;
// odd-size edges must neither read out of bounds nor repeat their last texel.
@compute @workgroup_size(8,8)
fn cs_downsample(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    let size = textureDimensions(field_input);
    var sum = vec3f(0);
    var count = 0.0;
    for (var y = 0u; y < 2u; y++) {
        for (var x = 0u; x < 2u; x++) {
            let p = id.xy * 2u + vec2u(x,y);
            if (any(p >= size)) { continue; }
            sum += textureLoad(field_input, vec2i(p), 0).rgb;
            count += 1;
        }
    }
    textureStore(field_output, vec2i(id.xy), vec4f(sum / max(count, 1), 0));
}

// A normalized tent reconstructs the broad halo. Convex merging keeps the
// original strength/white-field response instead of adding energy per level.
@compute @workgroup_size(8,8)
fn cs_upsample(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= textureDimensions(field_output))) { return; }
    let uv = (vec2f(id.xy) + 0.5) / vec2f(textureDimensions(field_output));
    let step = 1 / vec2f(textureDimensions(field_input));
    var halo = vec3f(0);
    for (var y = -1; y <= 1; y++) {
        for (var x = -1; x <= 1; x++) {
            let weight = f32((2 - abs(x)) * (2 - abs(y))) / 16;
            halo += textureSampleLevel(field_input, linear_sampler,
                uv + vec2f(f32(x), f32(y)) * step, 0).rgb * weight;
        }
    }
    let core = textureLoad(fine_input, vec2i(id.xy), 0).rgb;
    textureStore(field_output, vec2i(id.xy), vec4f(mix(core, halo, 0.5), 0));
}
