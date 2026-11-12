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
fn terrain_cell(grid: vec2f) -> vec4f {
    if (any(grid < vec2f(0)) || any(grid >= vec2f(11))) { return vec4f(0.0); }
    let cell = vec2u(grid);
    let i = cell.y * 12u + cell.x;
    let f = fract(grid);
    return mix(mix(params.fog_cells[i], params.fog_cells[i+1u], f.x),
               mix(params.fog_cells[i+12u], params.fog_cells[i+13u], f.x), f.y);
}

fn ground_fog_density(world: vec3f) -> f32 {
    let grid = (world.xz - params.fog_grid.xy) * params.fog_grid.z;
    let surface = terrain_cell(grid);
    if (surface.w < 0.45) { return 0.0; }
    let water = surface.y / surface.w;
    let hollow = surface.z / surface.w;
    let altitude = world.y - surface.x / surface.w;
    let thickness = 55.0 + water * 200.0 + hollow * 125.0;
    let vertical = smoothstep(-thickness * 0.8, 10.0, altitude) *
                   (1.0 - smoothstep(12.0, thickness, altitude));
    let breeze = params.fog_wind;
    let drift = world.xz - breeze.xy * breeze.z * breeze.w * 12.0;
    let veil = 0.75 + 0.16 * sin(dot(drift, vec2f(0.0053, 0.0031)) +
                                sin(dot(drift, vec2f(0.0021, -0.0047))));
    let edge = min(grid, vec2f(11) - grid);
    let coverage = smoothstep(0.0, 2.25, min(edge.x, edge.y)) * smoothstep(0.45, 0.95, surface.w);
    var wake = 1.0;
    for (var i = 0u; i < 2u; i++) {
        let actor = params.fog_actors[i];
        if (actor.w <= 0.0) { continue; }
        let distance = length(vec3f(world.x - actor.x, (world.y - actor.y) * 0.6, world.z - actor.z));
        wake *= mix(0.28, 1.0, smoothstep(actor.w * 0.3, actor.w * 2.3, distance));
    }
    return vertical * veil * coverage * wake * (0.10 + 1.60 * water + 0.95 * hollow);
}

fn wet_rock(color: vec4f, pixel: vec2i, center: DepthSample) -> vec4f {
    if (params.wet_light.w <= 0.0) { return color; }
    let world = (params.fog_world_from_view * vec4f(center.point, 1.0)).xyz;
    let grid = (world.xz - params.fog_grid.xy) * params.fog_grid.z;
    let surface = terrain_cell(grid);
    var wet = 0.0;
    if (surface.w > 0.65) {
        let water = surface.y / surface.w;
        let bank = smoothstep(0.03, 0.4, water) * (1.0 - smoothstep(0.48, 0.97, water));
        let altitude = world.y - surface.x / surface.w;
        let edge = min(grid, vec2f(11) - grid);
        wet = bank * (1.0 - smoothstep(24.0, 130.0, abs(altitude))) *
              smoothstep(0.0, 1.5, min(edge.x, edge.y));
    }
    for (var i = 0u; i < 6u; i++) {
        let spray = params.wet_spray[i];
        if (spray.w <= 0.0) { break; }
        let delta = world - spray.xyz;
        let reach = length(vec3f(delta.x, delta.y * 2.7, delta.z)) / spray.w;
        wet = max(wet, 1.0 - smoothstep(0.30, 1.0, reach));
    }
    if (wet <= 0.002) { return color; }
    // Depth and subdued chroma approximate exposed stone in the contact band.
    let chroma = max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
    wet *= (1.0 - smoothstep(0.11, 0.33, chroma)) * params.wet_light.w;
    let normal = surface_normal(pixel, center);
    if (dot(normal, normal) < 0.5) { return color; }
    let eye = normalize(-center.point);
    let facing = clamp(dot(normal, eye), 0.0, 1.0);
    let rim = pow(1.0 - facing, 4.0);
    let world_normal = (params.fog_world_from_view * vec4f(normal, 0.0)).xyz;
    let ray = params.wet_light.xyz - world;
    let world_eye = (params.fog_world_from_view * vec4f(eye, 0.0)).xyz;
    let half_ray = ray / max(length(ray), 1.0) + world_eye;
    let highlight = select(0.0, pow(clamp(dot(world_normal, half_ray / max(length(half_ray), 0.001)),
                                          0.0, 1.0), 32.0), any(params.wet_light.xyz != vec3f(0.0)));
    let sheen = vec3f(0.036, 0.044, 0.051) * rim + vec3f(0.075) * highlight;
    return vec4f(clamp(color.rgb * (1.0 - wet * 0.24) + sheen * wet, vec3f(0.0), vec3f(1.0)), color.a);
}

fn terrain_mist(point: vec3f) -> f32 {
    if (params.fog_grid.w <= 0.0) { return 0.0; }
    let eye = params.fog_world_from_view[3].xyz;
    let ray = (params.fog_world_from_view * vec4f(point, 0.0)).xyz;
    let distance = length(ray);
    let reach = min(distance, 2050.0);
    let direction = ray / max(distance, 1.0);
    var density = 0.0;
    for (var i = 0u; i < 4u; i++) {
        density += ground_fog_density(eye + direction * reach * (f32(i) + 0.5) * 0.25);
    }
    let surface = ground_fog_density(eye + ray + vec3f(0.0, 34.0, 0.0));
    return 1.0 - exp(-(density * 0.19 + surface * 0.57) * smoothstep(55.0, 680.0, reach));
}

fn atmosphere(color: vec4f, point: vec3f) -> vec4f {
    if (params.fog_color.a <= 0.0 && params.fog_range.z <= 0.0) { return color; }
    let span = max(params.fog_range.y - params.fog_range.x, 1.0);
    let distance = max(-point.z - params.fog_range.x, 0.0) / span;
    let amount = params.fog_color.a * (1.0 - exp(-2.0 * min(distance, 4.0)));
    var fog_target = params.fog_color.rgb;
    let sun = params.fog_light_direction;
    let sun_color = params.fog_light_color;
    if (sun.w > 0 && all(sun == sun) && all(abs(sun) < vec4f(1e20)) &&
        all(sun_color == sun_color) && all(abs(sun_color) < vec4f(1e20)) &&
        any(sun_color.rgb > vec3f(0))) {
        let ray_length = length(point);
        let sun_length = length(sun.xyz);
        if (ray_length > 0.0001 && ray_length < 1e20 && sun_length > 0.99 && sun_length < 1.01) {
            let cosine = clamp(dot(point / ray_length, sun.xyz / sun_length), -1.0, 1.0);
            // A peak-normalized forward lobe changes the fog target color only.
            // Density, distance interval and the existing atmosphere cap stay intact.
            let g = 0.65;
            let denominator = 1 + g * g - 2 * g * cosine;
            let phase = pow(((1-g)*(1-g)) / denominator, 1.5);
            let tint = clamp(sun.w, 0, 0.5) * clamp(phase, 0, 1);
            fog_target = mix(fog_target, clamp(sun_color.rgb, vec3f(0), vec3f(1)), tint);
        }
    }
    let distant = mix(color.rgb, fog_target, amount);
    let mist_target = mix(fog_target, vec3f(0.61, 0.66, 0.64), 0.49);
    return vec4f(mix(distant, mist_target, terrain_mist(point) * params.fog_range.z), color.a);
}
fn sunlight(color: vec4f, pixel: vec2i, center: vec3f) -> vec4f {
    let strength = clamp(params.sun_screen.z, 0, 0.25);
    if (strength <= 0 || params.sun_screen.w <= 0 ||
        any(params.sun_screen != params.sun_screen) || any(params.sun_color != params.sun_color)) { return color; }
    let size = vec2f(textureDimensions(scene_depth));
    let receiver_depth = textureLoad(scene_depth, pixel, 0).r;
    let background = abs(receiver_depth - params.background_depth) <= 0.000001;
    if (receiver_depth != receiver_depth || (!background &&
        (center.z >= -0.0001 || any(center != center) || any(abs(center) > vec3f(1e20))))) { return color; }
    let uv = (vec2f(pixel) + vec2f(0.5)) / size;
    let sun = params.sun_screen.xy;
    if (any(sun <= vec2f(0)) || any(sun >= vec2f(1))) { return color; }
    let delta = sun - uv;
    let radial = length(delta * vec2f(params.sun_screen.w, 1));
    let falloff = 1 - smoothstep(0.05, 0.65, radial);
    if (falloff <= 0) { return color; }
    let sun_edge = min(min(sun.x, 1-sun.x) * size.x, min(sun.y, 1-sun.y) * size.y);
    let edge_fade = smoothstep(0, 24, sun_edge);
    var visible = 0.0;
    var weights = 0.0;
    // Gather sky visibility toward the native sun. Opaque silhouettes create
    // radial interruptions; transparent clouds/effects are not in this snapshot.
    for (var tap = 0u; tap < 16u; tap++) {
        let t = (f32(tap) + 0.5) / 16;
        let sample_uv = uv + delta * t;
        let depth = textureLoad(scene_depth, vec2i(sample_uv * size), 0).r;
        let weight = 1 - t * 0.5;
        visible += select(0.0, weight, abs(depth - params.background_depth) <= 0.000001);
        weights += weight;
    }
    // Restrict foreground veiling while retaining sky and distant atmosphere.
    let receiver = select(1-exp(-max(-center.z,0)/2000), 1.0, background);
    let amount = strength * falloff * edge_fade * receiver * visible / max(weights,0.0001);
    let base = bounce_linear(clamp(color.rgb, vec3f(0), vec3f(1)));
    let added = (vec3f(1)-base) * bounce_linear(clamp(params.sun_color.rgb,vec3f(0),vec3f(1))) * amount;
    if (!any(added > vec3f(0))) { return color; }
    return vec4f(bounce_display(base+added),color.a);
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
            if (lighting_strength() > 0) { incoming += textureLoad(bounce_field, p, 0).rgb * weight; }
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
    let source = textureLoad(scene_color, pixel, 0);
    let sample = view_sample(pixel);
    let center = sample.point;
    if (center.z >= -0.0001) { return sunlight(source, pixel, center); }
    let color = wet_rock(source, pixel, sample);
    if (params.strength <= 0 && params.light_position.w <= 0 && lighting_strength() <= 0) {
        return sunlight(atmosphere(color, center), pixel, center);
    }
    let light = lighting(pixel, center);
    var shaded = color.rgb * light.a;
    if (params.shadow_range.z > 0.0) {
        let normal = surface_normal(pixel, sample);
        let facing = max(dot(normal, params.fog_light_direction.xyz), 0.0);
        let daylight = min(params.shadow_range.z / 0.21, 1.0);
        let shade = clamp((1.0 - light.a) * 2.7, 0.0, 1.0);
        let warm = vec3f(1.07, 1.035, 0.985);
        let cool = vec3f(0.965, 0.99, 1.065);
        let response = mix(vec3f(1.0), mix(warm, cool, shade), daylight);
        let fill = color.rgb * (1.0 - color.rgb) * facing * (1.0 - shade) *
                   vec3f(0.12, 0.095, 0.045) * daylight;
        shaded = clamp(shaded * response + fill, vec3f(0.0), vec3f(1.0));
    }
    if (lighting_strength() > 0 && any(light.rgb > vec3f(0))) {
        let base = bounce_linear(shaded);
        // Lit receiver color is a conservative reflectance proxy, not albedo.
        // Bound both energy and remaining headroom; pure black/white stay fixed.
        let strength = clamp(lighting_strength(), 0, 0.3);
        let added = min(bounce_linear(color.rgb) * clamp(light.rgb, vec3f(0), vec3f(1)) * strength,
                        (vec3f(1) - base) * strength);
        if (any(added > vec3f(0))) { shaded = bounce_display(base + added); }
    }
    return sunlight(atmosphere(vec4f(shaded, color.a), center), pixel, center);
}
