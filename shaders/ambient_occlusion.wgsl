@group(0) @binding(0) var scene_color: texture_2d<f32>;
@group(0) @binding(1) var scene_depth: texture_2d<f32>;
@group(0) @binding(2) var<uniform> params: AmbientParams;
@group(0) @binding(3) var ambient_field: texture_2d<f32>;
@group(0) @binding(4) var bounce_field: texture_2d<f32>;

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let triangle = array<vec2f, 3>(vec2f(-1,-1), vec2f(3,-1), vec2f(-1,3));
    return vec4f(triangle[index], 0, 1);
}
fn atmosphere(color: vec4f, point: vec3f) -> vec4f {
    if (params.fog_color.a <= 0.0) { return color; }
    let span = max(params.fog_range.y - params.fog_range.x, 1.0);
    let distance = max(-point.z - params.fog_range.x, 0.0) / span;
    let amount = params.fog_color.a * (1.0 - exp(-2.0 * min(distance, 4.0)));
    return vec4f(mix(color.rgb, params.fog_color.rgb, amount), color.a);
}
fn lighting(pixel: vec2i, center: vec3f) -> vec4f {
    // Half-resolution representatives sit at full pixel 2*i+1.
    let point = (vec2f(pixel) - 1) * 0.5;
    let base = vec2i(floor(point));
    let fraction = fract(point);
    let size = vec2i(textureDimensions(ambient_field));
    var sum = vec2f(0);
    var incoming = vec3f(0);
    var weights = 0.0;
    for (var y = 0; y < 2; y++) {
        for (var x = 0; x < 2; x++) {
            let p = clamp(base + vec2i(x,y), vec2i(0), size - 1);
            let field = textureLoad(ambient_field, p, 0);
            if (field.y <= 0) { continue; }
            let normal = decode_normal(field.zw);
            let difference = center - field_point(p);
            let separation = abs(dot(difference, normal));
            let edge = 1 - smoothstep(params.radius * 0.015, params.radius * 0.12, separation);
            let bilinear = select(1-fraction.x, fraction.x, x == 1) *
                           select(1-fraction.y, fraction.y, y == 1);
            let weight = bilinear * edge;
            sum += vec2f(field.x, max(field.y - 1, 0)) * weight;
            if (params.bounce.x > 0) { incoming += textureLoad(bounce_field, p, 0).rgb * weight; }
            weights += weight;
        }
    }
    if (weights < 0.0001) { return vec4f(0,0,0,1); }
    let value = sum / weights;
    let ambient = 1 - clamp((1 - value.x) * params.strength, 0, 0.5);
    let contact = 1 - clamp(value.y, 0, 0.5);
    return vec4f(incoming / weights, max(ambient * contact, 0.5));
}
@fragment
fn fs_ambient(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let pixel = vec2i(position.xy);
    let color = textureLoad(scene_color, pixel, 0);
    let center = view_point(pixel);
    if (center.z >= -0.0001) { return color; }
    if (params.strength <= 0 && params.light_position.w <= 0 && params.bounce.x <= 0) {
        return atmosphere(color, center);
    }
    let light = lighting(pixel, center);
    var shaded = color.rgb * light.a;
    if (params.bounce.x > 0 && any(light.rgb > vec3f(0))) {
        let base = bounce_linear(shaded);
        // Lit receiver color is a conservative reflectance proxy, not albedo.
        // Bound both energy and remaining headroom; pure black/white stay fixed.
        let strength = clamp(params.bounce.x, 0, 0.3);
        let added = min(bounce_linear(color.rgb) * clamp(light.rgb, vec3f(0), vec3f(1)) * strength,
                        (vec3f(1) - base) * strength);
        if (any(added > vec3f(0))) { shaded = bounce_display(base + added); }
    }
    return atmosphere(vec4f(shaded, color.a), center);
}
