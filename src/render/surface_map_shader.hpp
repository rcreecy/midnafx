#pragma once
namespace aurora::gfx::surface_maps {
inline constexpr char Shader[]=R"(
@group(0) @binding(0) var source: texture_2d<f32>;
@group(0) @binding(1) var linear_sampler: sampler;
@group(0) @binding(2) var output_map: texture_storage_2d<rgba8unorm,write>;
fn height_at(uv:vec2f,lod:f32)->f32 {
    return dot(textureSampleLevel(source,linear_sampler,uv,lod).rgb,vec3f(0.2126,0.7152,0.0722));
}
@compute @workgroup_size(8,8)
fn generate(@builtin(global_invocation_id) id:vec3u) {
    let size=textureDimensions(output_map);
    if (any(id.xy>=size)) { return; }
    let uv=(vec2f(id.xy)+0.5)/vec2f(size);
    let step=1.0/vec2f(size);
    let ratio=vec2f(textureDimensions(source))/vec2f(size);
    let lod=max(0.0,log2(max(ratio.x,ratio.y)));
    let c=height_at(uv,lod);
    let w=height_at(uv-vec2f(step.x,0.0),lod);
    let e=height_at(uv+vec2f(step.x,0.0),lod);
    let n=height_at(uv-vec2f(0.0,step.y),lod);
    let s=height_at(uv+vec2f(0.0,step.y),lod);
    let gradient=vec2f(e-w,s-n)*4.0;
    let normal=normalize(vec3f(-gradient,1.0));
    let contrast=length(vec2f(e-w,s-n));
    let roughness=clamp(0.5+contrast*0.6+abs(c-(w+e+n+s)*0.25)*0.8,0.5,1.0);
    textureStore(output_map,vec2i(id.xy),vec4f(normal.xy*0.5+0.5,roughness,c));
}
)";
}
