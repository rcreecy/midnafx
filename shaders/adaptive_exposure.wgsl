struct ExposureControls {
    seconds: f32,
    reset: u32,
    padding: vec2f,
}
@group(0) @binding(0) var scene: texture_2d<f32>;
@group(0) @binding(1) var previous: texture_2d<f32>;
@group(0) @binding(2) var result: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var<uniform> controls: ExposureControls;

var<workgroup> sums: array<vec3f, 256>;
var<workgroup> histogram: array<atomic<u32>, 64>;
var<workgroup> limits: vec2f;

fn sample_hash(input: u32) -> u32 {
    var value = input;
    value = ((value >> 16u) ^ value) * 0x45d9f3bu;
    value = ((value >> 16u) ^ value) * 0x45d9f3bu;
    return (value >> 16u) ^ value;
}

// Fixed stratification breaks coherent point-grid aliasing without changing
// the sample pattern over time. Black bars remain excluded from the meter.
@compute @workgroup_size(16, 16)
fn meter(@builtin(local_invocation_id) local: vec3u,
         @builtin(local_invocation_index) index: u32) {
    if (index < 64u) { atomicStore(&histogram[index], 0u); }
    workgroupBarrier();
    let center = (vec2f(local.xy) + 0.5) / 16.0;
    let centered = center * 2.0 - 1.0;
    let weight = (1.0 - 0.35 * dot(centered, centered)) / 16.0;
    let histogram_weight = u32(round(weight * 16384.0));
    let size = textureDimensions(scene);
    var samples: array<f32, 16>;
    for (var y = 0u; y < 4u; y++) {
        for (var x = 0u; x < 4u; x++) {
            let tap = y * 4u + x;
            let seed = index * 16u + tap;
            let offset = vec2f(f32(sample_hash(seed ^ 0x9e3779b9u) & 0x00ffffffu),
                               f32(sample_hash(seed ^ 0x68bc21ebu) & 0x00ffffffu)) / 16777216.0;
            let uv = (vec2f(local.xy) + (vec2f(f32(x), f32(y)) + offset) / 4.0) / 16.0;
            let pixel = min(vec2u(uv * vec2f(size)), size - vec2u(1));
            let color = textureLoad(scene, vec2i(pixel), 0).rgb;
            let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
            samples[tap] = -16.0;
            if (luma >= 0.004) {
                let value = log2(max(luma, 0.0001));
                samples[tap] = value;
                let bin = u32(clamp(floor((value + 8.0) * 8.0), 0.0, 63.0));
                atomicAdd(&histogram[bin], histogram_weight);
            }
        }
    }
    workgroupBarrier();
    if (index == 0u) {
        var total = 0u;
        for (var bin = 0u; bin < 64u; bin++) { total += atomicLoad(&histogram[bin]); }
        let lower_cut = total / 10u;
        let upper_cut = total - lower_cut;
        var cumulative = 0u;
        var lower = 0u;
        var upper = 63u;
        var found_lower = false;
        for (var bin = 0u; bin < 64u; bin++) {
            cumulative += atomicLoad(&histogram[bin]);
            if (!found_lower && cumulative > lower_cut) { lower = bin; found_lower = true; }
            if (upper_cut > 0u && cumulative >= upper_cut) { upper = bin; break; }
        }
        // Use bin edges, not centers: a uniform scene retains its exact luma.
        limits = vec2f(-8.0 + f32(lower) / 8.0, -8.0 + f32(upper + 1u) / 8.0);
    }
    workgroupBarrier();
    var sum = vec3f(0);
    for (var tap = 0u; tap < 16u; tap++) {
        let value = samples[tap];
        if (value < -8.0) { continue; }
        sum += vec3f(value * weight, clamp(value, limits.x, limits.y) * weight, weight);
    }
    sums[index] = sum;
    workgroupBarrier();
    for (var step = 128u; step > 0u; step /= 2u) {
        if (index < step) { sums[index] += sums[index + step]; }
        workgroupBarrier();
    }
    if (index != 0u) { return; }
    let raw_mean = exp2(sums[0].x / max(sums[0].z, 1.0));
    let mean = exp2(sums[0].y / max(sums[0].z, 1.0));
    if (sums[0].z < 16.0 || raw_mean < 0.02) {
        textureStore(result, vec2i(0), vec4f(0.0, raw_mean, 0.0, 1.0));
        return;
    }
    // Scene-code luminance: retain authored night/dungeon mood. This is not HDR.
    // Winsorize the outer ten percent of weighted samples. Small lights and
    // dark specks cannot dominate the target; the untrimmed mean gates fades.
    let desired = clamp(log2(0.32 / mean) * 0.55, -0.5, 0.75) * smoothstep(0.02, 0.06, raw_mean);
    let history = textureLoad(previous, vec2i(0), 0);
    let reset = controls.reset != 0u || history.z == 0.0;
    let before = select(history.x, 0.0, reset);
    let duration = select(1.2, 0.4, desired < before);
    let blend = 1.0 - exp(-clamp(controls.seconds, 0.0, 0.1) / duration);
    // The current fade gate also attenuates history, so a fade never stays lifted.
    let stops = mix(before, desired, blend) * smoothstep(0.02, 0.06, raw_mean);
    textureStore(result, vec2i(0), vec4f(stops, mean, 1.0, 1.0));
}
