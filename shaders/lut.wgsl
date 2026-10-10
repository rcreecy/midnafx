// Built-in scene-code RGB [0,1] domain; no implicit transfer conversion.
struct LookControls {
    intensity: f32,
    twilight: f32,
    padding: vec2f,
}
@group(0) @binding(6) var normal_look: texture_3d<f32>;
@group(0) @binding(7) var twilight_look: texture_3d<f32>;
@group(0) @binding(8) var look_sampler: sampler;
@group(0) @binding(9) var<uniform> look: LookControls;

fn apply_look(rgb: vec3f) -> vec3f {
    if (look.intensity <= 0.0) { return rgb; }
    let size = vec3f(textureDimensions(normal_look));
    let coord = (clamp(rgb, vec3f(0.0), vec3f(1.0)) * (size - 1.0) + 0.5) / size;
    let normal = textureSampleLevel(normal_look, look_sampler, coord, 0.0).rgb;
    var result = normal;
    if (look.twilight > 0.0) {
        let twilight = textureSampleLevel(twilight_look, look_sampler, coord, 0.0).rgb;
        result = mix(normal, twilight, clamp(look.twilight, 0.0, 1.0));
    }
    return mix(rgb, result, clamp(look.intensity, 0.0, 1.0));
}
