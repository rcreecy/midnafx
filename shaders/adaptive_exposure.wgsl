struct ExposureControls {
    seconds: f32,
    reset: u32,
    padding: vec2f,
}
@group(0) @binding(0) var scene: texture_2d<f32>;
@group(0) @binding(1) var previous: texture_2d<f32>;
@group(0) @binding(2) var result: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var<uniform> controls: ExposureControls;

var<workgroup> sums: array<vec2f, 256>;

// One fixed grid meters the original pre-HUD scene, independent of resolution.
// Ignoring black bars prevents cinematic framing from lifting the scene.
@compute @workgroup_size(16, 16)
fn meter(@builtin(local_invocation_id) local: vec3u,
         @builtin(local_invocation_index) index: u32) {
    let uv = (vec2f(local.xy) + 0.5) / 16.0;
    let size = textureDimensions(scene);
    let pixel = min(vec2u(uv * vec2f(size)), size - vec2u(1));
    let color = textureLoad(scene, vec2i(pixel), 0).rgb;
    let luma = dot(color, vec3f(0.2126, 0.7152, 0.0722));
    let centered = uv * 2.0 - 1.0;
    let weight = select(0.0, 1.0 - 0.35 * dot(centered, centered), luma >= 0.004);
    sums[index] = vec2f(log2(max(luma, 0.0001)) * weight, weight);
    workgroupBarrier();
    for (var step = 128u; step > 0u; step /= 2u) {
        if (index < step) { sums[index] += sums[index + step]; }
        workgroupBarrier();
    }
    if (index != 0u) { return; }
    let mean = exp2(sums[0].x / max(sums[0].y, 1.0));
    if (sums[0].y < 16.0 || mean < 0.02) {
        textureStore(result, vec2i(0), vec4f(0.0, mean, 0.0, 1.0));
        return;
    }
    // Scene-code luminance: retain authored night/dungeon mood. This is not HDR.
    let desired = clamp(log2(0.32 / mean) * 0.55, -0.5, 0.75) * smoothstep(0.02, 0.06, mean);
    let history = textureLoad(previous, vec2i(0), 0);
    let reset = controls.reset != 0u || history.z == 0.0;
    let before = select(history.x, 0.0, reset);
    let duration = select(1.2, 0.4, desired < before);
    let blend = 1.0 - exp(-clamp(controls.seconds, 0.0, 0.1) / duration);
    // The current fade gate also attenuates history, so a fade never stays lifted.
    let stops = mix(before, desired, blend) * smoothstep(0.02, 0.06, mean);
    textureStore(result, vec2i(0), vec4f(stops, mean, 1.0, 1.0));
}
