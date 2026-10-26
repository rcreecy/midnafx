fn aa_luma(color: vec3f) -> f32 {
    return dot(color, vec3f(0.299, 0.587, 0.114));
}

fn aa_load(source: texture_2d<f32>, position: vec2i) -> vec3f {
    return textureLoad(source, clamp(position, vec2i(0), vec2i(textureDimensions(source)) - 1), 0).rgb;
}

// One shared spatial filter for ordinary and water entry points. No history,
// extra render target, sampler binding or asset classification is required.
fn edge_aa(source: texture_2d<f32>, center: vec3f, position: vec2i, detail: bool) -> vec3f {
    let north = aa_load(source, position + vec2i(0, -1));
    let east = aa_load(source, position + vec2i(1, 0));
    let south = aa_load(source, position + vec2i(0, 1));
    let west = aa_load(source, position + vec2i(-1, 0));
    let c = aa_luma(center);
    let n = aa_luma(north); let e = aa_luma(east);
    let s = aa_luma(south); let w = aa_luma(west);
    let low = min(c, min(min(n, e), min(s, w)));
    let high = max(c, max(max(n, e), max(s, w)));
    let contrast = high - low;
    if (contrast < max(0.0312, high * 0.125)) {
        if (detail) { return detail_from_neighbors(center, north, east, south, west); }
        return center;
    }
    let last = vec2i(textureDimensions(source)) - 1;
    // An incomplete 3x3 neighborhood cannot distinguish a clipped corner from
    // a checker/ridge. Keep the outermost texels instead of inventing coverage.
    if (any(position == vec2i(0)) || any(position == last)) { return center; }

    let nw = aa_luma(aa_load(source, position + vec2i(-1, -1)));
    let ne = aa_luma(aa_load(source, position + vec2i(1, -1)));
    let sw = aa_luma(aa_load(source, position + vec2i(-1, 1)));
    let se = aa_luma(aa_load(source, position + vec2i(1, 1)));
    let horizontal = abs(nw + sw - 2 * w) + 2 * abs(n + s - 2 * c) + abs(ne + se - 2 * e);
    let vertical = abs(nw + ne - 2 * n) + 2 * abs(w + e - 2 * c) + abs(sw + se - 2 * s);
    let coverage = abs((n + e + s + w) * 0.25 - c) / max(contrast, 0.00001);
    let subpixel = min(0.40, coverage * coverage * 0.75);
    if (abs(horizontal - vertical) <= max(horizontal, vertical) * 0.00001) {
        // A thin diagonal ridge (or isolated texel) can meet both axes while
        // all diagonal samples still match the center. It has no area edge
        // to reconstruct; avoid spreading it into neighboring texels.
        let diagonal_change = max(max(abs(nw - c), abs(ne - c)), max(abs(sw - c), abs(se - c)));
        if (diagonal_change < contrast * 0.25) { return center; }
        // There is no preferred axis at symmetric corners/diagonal steps.
        // Average eligible normals rather than picking a different answer
        // when the same image is transposed. Reject thin ridges per axis.
        let gx = vec2f(abs(w - c), abs(e - c));
        let gy = vec2f(abs(n - c), abs(s - c));
        let eligible_x = min(gx.x, gx.y) < max(gx.x, gx.y) * 0.75;
        let eligible_y = min(gy.x, gy.y) < max(gy.x, gy.y) * 0.75;
        let count = u32(eligible_x) + u32(eligible_y);
        if (count == 0u) { return center; }
        let x_color = select(east, west, gx.x >= gx.y);
        let y_color = select(south, north, gy.x >= gy.y);
        let normal_color = (select(vec3f(0), x_color, eligible_x) +
                            select(vec3f(0), y_color, eligible_y)) / f32(count);
        return mix(center, normal_color, subpixel);
    }
    let along_x = horizontal >= vertical;
    let minus_luma = select(w, n, along_x);
    let plus_luma = select(e, s, along_x);
    let minus_gradient = abs(minus_luma - c);
    let plus_gradient = abs(plus_luma - c);
    let gradient = max(minus_gradient, plus_gradient);

    // Opposing, comparable gradients identify thin ridges/checker texels.
    // Do not turn a one-pixel line into a wider, dimmer feature.
    if (min(minus_gradient, plus_gradient) >= gradient * 0.75) { return center; }
    let toward_minus = minus_gradient >= plus_gradient;
    let normal = select(vec2i(1, 0), vec2i(0, 1), along_x) * select(1, -1, toward_minus);
    let tangent = select(vec2i(0, 1), vec2i(1, 0), along_x);
    let neighbor = select(select(east, south, along_x), select(west, north, along_x), toward_minus);
    let mean = (c + aa_luma(neighbor)) * 0.5;
    var distances = vec2f(12);
    var deltas = vec2f(0);
    var found = vec2<bool>(false);
    var done = vec2<bool>(false);
    // Dense near searches preserve short stair steps; bounded farther searches
    // distinguish them from long, already aligned edges.
    let steps = array<i32,7>(1, 2, 3, 4, 6, 8, 12);
    for (var step = 0u; step < 7u; step++) {
        for (var side = 0u; side < 2u; side++) {
            if (done[side]) { continue; }
            let p = position + tangent * steps[step] * select(-1, 1, side == 1u);
            let q = p + normal;
            if (any(p < vec2i(0)) || any(p > last) || any(q < vec2i(0)) || any(q > last)) {
                done[side] = true;
                continue;
            }
            let delta = (aa_luma(aa_load(source, p)) + aa_luma(aa_load(source, q))) * 0.5 - mean;
            if (abs(delta) >= gradient * 0.25) {
                distances[side] = f32(steps[step]);
                deltas[side] = delta;
                found[side] = true;
                done[side] = true;
            }
        }
        if (all(done)) { break; }
    }
    // A framebuffer boundary is not an edge endpoint. An unbroken straight
    // edge therefore retains its exact source texel, regardless of contrast.
    if (!any(found)) { return center; }
    let nearest = select(1u, 0u, distances.x <= distances.y);
    let valid_endpoint = found[nearest] && ((deltas[nearest] < 0) != (c - mean < 0));
    let coverage_endpoint = (found.x && ((deltas.x < 0) != (c - mean < 0))) ||
                            (found.y && ((deltas.y < 0) != (c - mean < 0)));
    let geometric = select(0.0, 0.5 - distances[nearest] / (distances.x + distances.y), valid_endpoint);
    // Symmetric one-pixel stair steps need subpixel coverage even when endpoint
    // distances match. The endpoint gate keeps this term off aligned edges.
    let blend = min(0.40, max(geometric, select(0.0, subpixel, coverage_endpoint)));
    // Do not sharpen a filtered edge against unfiltered neighbors.
    return mix(center, neighbor, blend);
}
