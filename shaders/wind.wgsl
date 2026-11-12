fn wind_gust(point: vec2f, wind: vec4f) -> f32 {
    let along = dot(point, wind.xy);
    let crosswind = dot(point, vec2f(-wind.y, wind.x));
    let front = sin(along * 0.0027 - wind.w * 1.92 + sin(crosswind * 0.0015) * 0.73);
    let rolling = sin(along * 0.0011 + crosswind * 0.0018 - wind.w * 1.10);
    let pulse = smoothstep(-0.65, 0.87, front * 0.73 + rolling * 0.27);
    return wind.z * mix(0.21, 1.0, pulse);
}
