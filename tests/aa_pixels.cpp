#include "midnafx_shader.hpp"
#include "render/lut.hpp"
#include <webgpu/webgpu.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iterator>
#include <thread>
#include <string>
#include <utility>
#include <vector>

namespace {
using Pixel = std::array<unsigned char, 4>;
struct WaterTestInput {
    std::array<float, 112> parameters{};
    std::vector<float> scene_depth, surface_depth;
    std::vector<Pixel> mask, behind_water;
};

// Keep failure output bounded while distinguishing missing pixels, channel
// corruption, and coordinate errors on remote GPU runners.
void report_difference(const char* label, unsigned width,
                       const std::vector<Pixel>& expected,
                       const std::vector<Pixel>& actual) {
    if (actual.size() != expected.size()) {
        std::fprintf(stderr, "%s: expected %zu pixels, received %zu\n",
                     label, expected.size(), actual.size());
        return;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == actual[i]) continue;
        const auto& e = expected[i];
        const auto& a = actual[i];
        std::fprintf(stderr,
                     "%s: first difference at (%zu,%zu), expected RGBA=%u,%u,%u,%u; actual=%u,%u,%u,%u\n",
                     label, i % width, i / width,
                     unsigned(e[0]), unsigned(e[1]), unsigned(e[2]), unsigned(e[3]),
                     unsigned(a[0]), unsigned(a[1]), unsigned(a[2]), unsigned(a[3]));
        return;
    }
}

bool wait(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!info.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &info, 0);
        if (status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) {
            std::fprintf(stderr, "GPU wait failed: %u\n", unsigned(status));
            std::exit(1);
        }
        if (std::chrono::steady_clock::now() > deadline) {
            // Callback userdata lives on the stack: do not return with it pending.
            std::fprintf(stderr, "GPU callback timed out\n");
            std::exit(1);
        }
        if (!info.completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

std::vector<Pixel> render(WGPUInstance instance, WGPUDevice device, const char* entry,
                          unsigned width, unsigned height, const std::vector<Pixel>& pixels,
                          float detail_strength = 0.0f, float look_strength = -1.0f,
                          float gain = 1.0f, unsigned table = 0, float twilight = 0.0f,
                          WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm,
                          float exposure_stops = 0.0f, float bloom_value = 0.0f,
                          bool water_active = true, const WaterTestInput* water_input = nullptr) {
    if (water_input && (water_input->scene_depth.size() != pixels.size() ||
        water_input->surface_depth.size() != pixels.size() || water_input->mask.size() != pixels.size() ||
        (!water_input->behind_water.empty() && water_input->behind_water.size() != pixels.size()))) return {};
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    auto queue = wgpuDeviceGetQueue(device);
    WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
    td.size = {width, height, 1};
    td.format = format;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    auto source = wgpuDeviceCreateTexture(device, &td);
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    auto target = wgpuDeviceCreateTexture(device, &td);
    auto source_view = wgpuTextureCreateView(source, nullptr);
    auto target_view = wgpuTextureCreateView(target, nullptr);
    WGPUTexelCopyTextureInfo upload = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    upload.texture = source;
    WGPUTexelCopyBufferLayout upload_layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    upload_layout.bytesPerRow = width * 4;
    upload_layout.rowsPerImage = height;
    wgpuQueueWriteTexture(queue, &upload, pixels.data(), pixels.size() * sizeof(Pixel),
                          &upload_layout, &td.size);
    auto exposure_desc = td;
    exposure_desc.size = {1, 1, 1};
    exposure_desc.format = WGPUTextureFormat_RGBA16Float;
    exposure_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    auto exposure = wgpuDeviceCreateTexture(device, &exposure_desc);
    auto exposure_view = wgpuTextureCreateView(exposure, nullptr);
    auto bloom_desc = exposure_desc;
    bloom_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    auto bloom = wgpuDeviceCreateTexture(device, &bloom_desc);
    auto bloom_view = wgpuTextureCreateView(bloom, nullptr);
    WGPUSamplerDescriptor bloom_sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    bloom_sampler_desc.minFilter = WGPUFilterMode_Linear;
    bloom_sampler_desc.magFilter = WGPUFilterMode_Linear;
    auto bloom_sampler = wgpuDeviceCreateSampler(device, &bloom_sampler_desc);
    const std::array<std::uint16_t, 4> exposure_value{
        static_cast<std::uint16_t>(midnafx::lut::half(std::abs(exposure_stops)) |
                                   (exposure_stops < 0 ? 0x8000 : 0)),
        0, 0, midnafx::lut::half(1.0f)};
    auto exposure_upload = upload;
    exposure_upload.texture = exposure;
    auto exposure_layout = upload_layout;
    exposure_layout.bytesPerRow = 8;
    exposure_layout.rowsPerImage = 1;
    wgpuQueueWriteTexture(queue, &exposure_upload, exposure_value.data(), sizeof(exposure_value),
                          &exposure_layout, &exposure_desc.size);
    const std::array<std::uint16_t, 4> bloom_pixel{
        midnafx::lut::half(bloom_value), midnafx::lut::half(bloom_value), midnafx::lut::half(bloom_value), 0};
    auto bloom_upload = exposure_upload;
    bloom_upload.texture = bloom;
    wgpuQueueWriteTexture(queue, &bloom_upload, bloom_pixel.data(), sizeof(bloom_pixel),
                          &exposure_layout, &bloom_desc.size);

    // Production Grading layout, with neutral tone controls and selectable detail.
    const bool water = std::strncmp(entry, "fs_absorption", 13) == 0;
    const std::array<float, 12> neutral{gain, gain, gain, 0, 1, 1, 1, 0, detail_strength, 1, 0, 0};
    std::vector<float> parameters(water ? 112 : 12);
    std::copy(neutral.begin(), neutral.end(), parameters.begin() + (water ? 84 : 0));
    std::array<WGPUTexture, 3> water_textures{};
    std::array<WGPUTextureView, 3> water_views{};
    WGPUTexture behind_water = nullptr;
    WGPUTextureView behind_water_view = nullptr;
    if (water) {
        for (unsigned matrix = 0; matrix < 3; ++matrix)
            for (unsigned diagonal = 0; diagonal < 4; ++diagonal)
                parameters[matrix * 16 + diagonal * 5] = 1;
        parameters[48] = 2; parameters[49] = 1; parameters[50] = 0.65f;
        parameters[52] = 0.2f; parameters[53] = 0.3f; parameters[54] = 0.4f;
        parameters[56] = 0.1f; parameters[57] = 0.2f; parameters[58] = 0.3f;
        parameters[61] = 1;
        for (unsigned diagonal = 0; diagonal < 4; ++diagonal) parameters[96 + diagonal * 5] = 1;
        if (water_input) std::copy(water_input->parameters.begin(), water_input->parameters.end(), parameters.begin());
        auto texture_desc = td;
        texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        for (unsigned i = 0; i < 3; ++i) {
            texture_desc.format = water_input && i < 2 ? WGPUTextureFormat_R32Float : format;
            water_textures[i] = wgpuDeviceCreateTexture(device, &texture_desc);
            water_views[i] = wgpuTextureCreateView(water_textures[i], nullptr);
            std::vector<Pixel> contents(pixels.size(), Pixel{static_cast<unsigned char>(i == 0 ? 192 : 64), 0, 0, 255});
            if (!water_active && i == 2) for (auto& pixel : contents) pixel[3] = 0;
            upload.texture = water_textures[i];
            if (water_input && i < 2) {
                const auto& depths = i == 0 ? water_input->scene_depth : water_input->surface_depth;
                wgpuQueueWriteTexture(queue, &upload, depths.data(), depths.size() * sizeof(float), &upload_layout, &td.size);
            } else {
                const auto& data = water_input ? water_input->mask : contents;
                wgpuQueueWriteTexture(queue, &upload, data.data(), data.size() * sizeof(Pixel), &upload_layout, &td.size);
            }
        }
        if (water_input && !water_input->behind_water.empty()) {
            texture_desc.format = format;
            behind_water = wgpuDeviceCreateTexture(device, &texture_desc);
            behind_water_view = wgpuTextureCreateView(behind_water, nullptr);
            upload.texture = behind_water;
            wgpuQueueWriteTexture(queue, &upload, water_input->behind_water.data(),
                water_input->behind_water.size() * sizeof(Pixel), &upload_layout, &td.size);
        }
    }
    WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
    bd.size = parameters.size() * sizeof(float);
    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    auto uniform = wgpuDeviceCreateBuffer(device, &bd);
    wgpuQueueWriteBuffer(queue, uniform, 0, parameters.data(), bd.size);
    midnafx::lut::Texture lattice;
    midnafx::lut::Texture twilight_lattice;
    WGPUSampler look_sampler = nullptr;
    WGPUBuffer look_uniform = nullptr;
    if (look_strength >= 0) {
        lattice.create(device, midnafx::lut::lattice(table));
        twilight_lattice.create(device, midnafx::lut::lattice(table ? 2 : 0));
        look_sampler = midnafx::lut::sampler(device);
        midnafx::lut::Controls controls{look_strength, twilight};
        bd.size = sizeof(controls);
        look_uniform = wgpuDeviceCreateBuffer(device, &bd);
        wgpuQueueWriteBuffer(queue, look_uniform, 0, &controls, sizeof(controls));
    }
    const unsigned stride = (width * 4 + 255) & ~255u;
    bd.size = stride * height;
    bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    auto readback = wgpuDeviceCreateBuffer(device, &bd);

    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {water ? midnafx::render::water_thickness_shader : midnafx::render::grading_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor sd = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    sd.nextInChain = &wgsl.chain;
    auto shader = wgpuDeviceCreateShaderModule(device, &sd);
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
    color.format = td.format;
    color.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    const std::string entry_name = std::string(entry) + (look_strength >= 0 ? "_lut" : "");
    fragment.entryPoint = {entry_name.c_str(), WGPU_STRLEN};
    fragment.targetCount = 1;
    fragment.targets = &color;
    WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pd.vertex.module = shader;
    pd.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pd.fragment = &fragment;
    auto pipeline = wgpuDeviceCreateRenderPipeline(device, &pd);
    auto layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    WGPUBindGroupEntry entries[13];
    for (auto& item : entries) item = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].textureView = source_view;
    unsigned count = 1;
    if (water) {
        for (unsigned i = 0; i < 3; ++i) {
            entries[count].binding = count;
            entries[count++].textureView = water_views[i];
        }
    }
    entries[count].binding = water ? 4 : 1;
    entries[count].buffer = uniform;
    entries[count++].size = parameters.size() * sizeof(float);
    if (water) {
        entries[count].binding = 5;
        entries[count++].textureView = behind_water_view ? behind_water_view : source_view;
    }
    if (look_strength >= 0) {
        entries[count].binding = 6; entries[count++].textureView = lattice.view;
        entries[count].binding = 7; entries[count++].textureView = twilight_lattice.view;
        entries[count].binding = 8; entries[count++].sampler = look_sampler;
        entries[count].binding = 9; entries[count].buffer = look_uniform;
        entries[count++].size = sizeof(midnafx::lut::Controls);
    }
    entries[count].binding = 10;
    entries[count++].textureView = exposure_view;
    entries[count].binding = 11; entries[count++].textureView = bloom_view;
    entries[count].binding = 12; entries[count++].sampler = bloom_sampler;
    WGPUBindGroupDescriptor bgd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bgd.layout = layout;
    bgd.entryCount = count;
    bgd.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(device, &bgd);
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    attachment.view = target_view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &attachment;
    auto pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    WGPUTexelCopyTextureInfo copy = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    copy.texture = target;
    WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    destination.buffer = readback;
    destination.layout.bytesPerRow = stride;
    destination.layout.rowsPerImage = height;
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &copy, &destination, &td.size);
    auto command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &command);
    bool mapped = false;
    WGPUBufferMapCallbackInfo map_info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    map_info.mode = WGPUCallbackMode_WaitAnyOnly;
    map_info.userdata1 = &mapped;
    map_info.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUMapAsyncStatus_Success;
    };
    const bool map_waited = wait(instance, wgpuBufferMapAsync(readback, WGPUMapMode_Read,
                                                             0, bd.size, map_info));
    std::vector<Pixel> output;
    if (map_waited && mapped) {
        const auto* data = static_cast<const unsigned char*>(
            wgpuBufferGetConstMappedRange(readback, 0, bd.size));
        if (data) {
            output.resize(pixels.size());
            for (unsigned y = 0; y < height; ++y)
                std::memcpy(output.data() + y * width, data + y * stride, width * 4);
        }
        wgpuBufferUnmap(readback);
    }
    bool valid = false;
    WGPUPopErrorScopeCallbackInfo scope = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    scope.mode = WGPUCallbackMode_WaitAnyOnly;
    scope.userdata1 = &valid;
    scope.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type,
                         WGPUStringView message, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success &&
                                   type == WGPUErrorType_NoError;
        if (!*static_cast<bool*>(user))
            std::fprintf(stderr, "GPU validation: %.*s\n", int(message.length), message.data);
    };
    if (!wait(instance, wgpuDevicePopErrorScope(device, scope)) || !valid) output.clear();
    wgpuCommandBufferRelease(command);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(layout);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(shader);
    wgpuBufferRelease(readback);
    wgpuBufferRelease(uniform);
    if (look_uniform) wgpuBufferRelease(look_uniform);
    if (look_sampler) wgpuSamplerRelease(look_sampler);
    wgpuTextureViewRelease(target_view);
    wgpuTextureViewRelease(source_view);
    wgpuTextureRelease(target);
    wgpuTextureRelease(source);
    wgpuTextureViewRelease(exposure_view);
    wgpuTextureRelease(exposure);
    wgpuTextureViewRelease(bloom_view);
    wgpuTextureRelease(bloom);
    wgpuSamplerRelease(bloom_sampler);
    for (auto view : water_views) if (view) wgpuTextureViewRelease(view);
    for (auto texture : water_textures) if (texture) wgpuTextureRelease(texture);
    if (behind_water_view) wgpuTextureViewRelease(behind_water_view);
    if (behind_water) wgpuTextureRelease(behind_water);
    wgpuQueueRelease(queue);
    return output;
}

struct WaterScene {
    WaterTestInput input;
    std::vector<Pixel> pixels;
    std::vector<float> water_distance;
};
float reflection_depth(float distance, bool reversed = false) {
    const float depth = 100.0f / 99 - (100.0f / 99) / distance;
    return reversed ? 1 - depth : depth;
}
WaterScene reflection_scene(unsigned width, unsigned height, Pixel donor,
                            bool reversed = false, float eye_y = 1, float pitch_degrees = 15) {
    WaterScene scene;
    const size_t count = size_t(width) * height;
    auto& input = scene.input;
    auto& p = input.parameters;
    const float angle = pitch_degrees * std::acos(-1.0f) / 180;
    const float cosine = std::cos(angle), sine = std::sin(angle);
    std::array<float,16> inverse_projection{}, world_from_view{};
    inverse_projection[0] = float(width) / height;
    inverse_projection[5] = 1;
    inverse_projection[11] = reversed ? 99.0f / 100 : -99.0f / 100;
    inverse_projection[14] = -1;
    inverse_projection[15] = reversed ? 1.0f / 100 : 1;
    std::copy(inverse_projection.begin(), inverse_projection.end(), p.begin());
    world_from_view[0] = 1; world_from_view[5] = cosine; world_from_view[6] = -sine;
    world_from_view[9] = sine; world_from_view[10] = cosine;
    world_from_view[13] = eye_y; world_from_view[15] = 1;
    for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned k = 0; k < 4; ++k)
                p[16 + column * 4 + row] += world_from_view[k * 4 + row] * inverse_projection[column * 4 + k];
    p[32] = 1; p[37] = cosine; p[38] = sine; p[41] = -sine; p[42] = cosine;
    p[45] = -cosine * eye_y; p[46] = -sine * eye_y; p[47] = 1;
    p[48] = 12; p[49] = reversed ? 0.0f : 1.0f;
    p[61] = 1;
    p[64] = 0.2f; p[65] = 0.3f; p[66] = 0.4f;
    p[68] = 1; p[69] = 1; p[70] = 12;
    p[84] = p[85] = p[86] = p[88] = p[89] = p[90] = p[93] = 1;
    p[96] = float(height) / width; p[101] = 1;
    p[106] = reversed ? 1.0f / 99 : -100.0f / 99;
    p[107] = -1; p[110] = reversed ? 100.0f / 99 : -100.0f / 99;
    input.scene_depth.resize(count, p[49]); input.surface_depth.resize(count, p[49]);
    input.mask.resize(count, Pixel{0,0,0,0}); input.behind_water.resize(count, donor);
    scene.pixels.resize(count, donor); scene.water_distance.resize(count, 0);
    // Intersect independent analytic rays with water Y=0 and wall Z=-6.
    // Camera coordinates use a right-handed, 90-degree vertical perspective.
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            const size_t i = size_t(y) * width + x;
            const float ray_y = 1 - 2 * (y + 0.5f) / height;
            const float world_y = cosine * ray_y - sine;
            const float world_z = -sine * ray_y - cosine;
            const float wall = world_z < 0 ? -6 / world_z : 1e30f;
            const float water = std::abs(world_y) > 0.000001f ? -eye_y / world_y : -1;
            if (wall >= 1 && wall <= 100) input.scene_depth[i] = reflection_depth(wall, reversed);
            if (water >= 1 && water <= 100) {
                input.surface_depth[i] = reflection_depth(water, reversed);
                scene.water_distance[i] = water;
                if (water < wall) {
                    input.mask[i][3] = 255;
                    scene.pixels[i] = {80,100,110,0};
                }
            }
            scene.pixels[i][3] = static_cast<unsigned char>((i * 37 + 11) % 256);
        }
    return scene;
}
bool reflection_alpha(const WaterScene& scene, const std::vector<Pixel>& output) {
    if (output.size() != scene.pixels.size()) return false;
    for (size_t i = 0; i < output.size(); ++i)
        if (output[i][3] != scene.pixels[i][3]) return false;
    return true;
}

struct MeterStep {
    std::vector<Pixel> pixels;
    float seconds;
    std::uint32_t reset;
};
using MeterValue = std::array<float, 4>;

float decode_half(std::uint16_t value) {
    const unsigned exponent = (value >> 10) & 31;
    const unsigned fraction = value & 1023;
    const float magnitude = exponent == 31 ?
        (fraction ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity()) :
        exponent == 0 ? std::ldexp(float(fraction), -24) :
                       std::ldexp(float(1024 + fraction), int(exponent) - 25);
    return value & 0x8000 ? -magnitude : magnitude;
}

// Each write precedes its own submission. History alternates GPU textures;
// snapshots occupy separate aligned rows and are read only after the last step.
std::vector<MeterValue> meter(WGPUInstance instance, WGPUDevice device,
                              const std::vector<MeterStep>& steps) {
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    auto queue = wgpuDeviceGetQueue(device);
    WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
    td.size = {16, 16, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    auto source = wgpuDeviceCreateTexture(device, &td);
    auto source_view = wgpuTextureCreateView(source, nullptr);
    td.size = {1, 1, 1};
    td.format = WGPUTextureFormat_RGBA16Float;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding |
               WGPUTextureUsage_CopySrc;
    std::array<WGPUTexture, 2> history{};
    std::array<WGPUTextureView, 2> views{};
    for (unsigned i = 0; i < 2; ++i) {
        history[i] = wgpuDeviceCreateTexture(device, &td);
        views[i] = wgpuTextureCreateView(history[i], nullptr);
    }
    struct Controls { float seconds; std::uint32_t reset; float padding[2]{}; };
    static_assert(sizeof(Controls) == 16);
    WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
    bd.size = sizeof(Controls);
    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    auto uniform = wgpuDeviceCreateBuffer(device, &bd);
    bd.size = steps.size() * 256;
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    auto readback = wgpuDeviceCreateBuffer(device, &bd);
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {midnafx::render::adaptive_exposure_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor sd = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    sd.nextInChain = &wgsl.chain;
    auto shader = wgpuDeviceCreateShaderModule(device, &sd);
    WGPUComputePipelineDescriptor pd = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pd.compute.module = shader;
    pd.compute.entryPoint = {"meter", WGPU_STRLEN};
    auto pipeline = wgpuDeviceCreateComputePipeline(device, &pd);
    auto layout = wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
    std::array<WGPUBindGroup, 2> groups{};
    for (unsigned i = 0; i < 2; ++i) {
        WGPUBindGroupEntry entries[4];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entries[0].binding = 0; entries[0].textureView = source_view;
        entries[1].binding = 1; entries[1].textureView = views[i];
        entries[2].binding = 2; entries[2].textureView = views[1 - i];
        entries[3].binding = 3; entries[3].buffer = uniform; entries[3].size = sizeof(Controls);
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = layout; desc.entryCount = 4; desc.entries = entries;
        groups[i] = wgpuDeviceCreateBindGroup(device, &desc);
    }
    for (size_t i = 0; i < steps.size(); ++i) {
        const Controls controls{steps[i].seconds, steps[i].reset};
        wgpuQueueWriteBuffer(queue, uniform, 0, &controls, sizeof(controls));
        WGPUTexelCopyTextureInfo upload = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        upload.texture = source;
        WGPUTexelCopyBufferLayout upload_layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
        upload_layout.bytesPerRow = 16 * 4; upload_layout.rowsPerImage = 16;
        const WGPUExtent3D source_size{16, 16, 1};
        wgpuQueueWriteTexture(queue, &upload, steps[i].pixels.data(),
                              steps[i].pixels.size() * sizeof(Pixel), &upload_layout, &source_size);
        auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
        auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
        wgpuComputePassEncoderSetPipeline(pass, pipeline);
        wgpuComputePassEncoderSetBindGroup(pass, 0, groups[i % 2], 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
        wgpuComputePassEncoderEnd(pass);
        WGPUTexelCopyTextureInfo copy = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        copy.texture = history[1 - i % 2];
        WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
        destination.buffer = readback;
        destination.layout.offset = i * 256;
        destination.layout.bytesPerRow = 256; destination.layout.rowsPerImage = 1;
        wgpuCommandEncoderCopyTextureToBuffer(encoder, &copy, &destination, &td.size);
        auto command = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);
        wgpuComputePassEncoderRelease(pass);
        wgpuCommandEncoderRelease(encoder);
    }
    bool mapped = false;
    WGPUBufferMapCallbackInfo map_info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    map_info.mode = WGPUCallbackMode_WaitAnyOnly; map_info.userdata1 = &mapped;
    map_info.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUMapAsyncStatus_Success;
    };
    wait(instance, wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, bd.size, map_info));
    std::vector<MeterValue> output;
    if (mapped) {
        const auto* bytes = static_cast<const unsigned char*>(
            wgpuBufferGetConstMappedRange(readback, 0, bd.size));
        if (bytes) {
            output.resize(steps.size());
            for (size_t i = 0; i < steps.size(); ++i) {
                std::array<std::uint16_t, 4> half{};
                std::memcpy(half.data(), bytes + i * 256, sizeof(half));
                for (unsigned c = 0; c < 4; ++c) output[i][c] = decode_half(half[c]);
            }
        }
        wgpuBufferUnmap(readback);
    }
    bool valid = false;
    WGPUPopErrorScopeCallbackInfo scope = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    scope.mode = WGPUCallbackMode_WaitAnyOnly; scope.userdata1 = &valid;
    scope.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type,
                         WGPUStringView message, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success &&
                                   type == WGPUErrorType_NoError;
        if (!*static_cast<bool*>(user))
            std::fprintf(stderr, "Exposure GPU validation: %.*s\n", int(message.length), message.data);
    };
    wait(instance, wgpuDevicePopErrorScope(device, scope));
    if (!valid) output.clear();
    for (auto group : groups) wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(layout);
    wgpuComputePipelineRelease(pipeline);
    wgpuShaderModuleRelease(shader);
    wgpuBufferRelease(readback);
    wgpuBufferRelease(uniform);
    for (auto view : views) wgpuTextureViewRelease(view);
    for (auto texture : history) wgpuTextureRelease(texture);
    wgpuTextureViewRelease(source_view);
    wgpuTextureRelease(source);
    wgpuQueueRelease(queue);
    return output;
}

struct HalfPlane {
    std::vector<Pixel> source;
    std::vector<double> coverage;
};

// A binary center sample is the input. Independent 64x64 coverage of the
// declared continuous half-plane is the reference, without consulting AA output.
HalfPlane half_plane(unsigned width, unsigned height, double slope, bool transpose) {
    HalfPlane result{std::vector<Pixel>(width * height), std::vector<double>(width * height)};
    const auto side = [&](double x, double y) {
        return transpose ? x - width * 0.5 - slope * (y - height * 0.5) :
                           y - height * 0.5 - slope * (x - width * 0.5);
    };
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            const size_t i = y * width + x;
            const unsigned char level = side(x + 0.5, y + 0.5) >= 0 ? 255 : 0;
            result.source[i] = {level, level, level, static_cast<unsigned char>((x * 37 + y * 19) % 256)};
            const double corners[]{side(x, y), side(x + 1, y), side(x, y + 1), side(x + 1, y + 1)};
            if (*std::min_element(std::begin(corners), std::end(corners)) >= 0) result.coverage[i] = 255;
            else if (*std::max_element(std::begin(corners), std::end(corners)) <= 0) result.coverage[i] = 0;
            else {
                double inside = 0;
                for (unsigned sy = 0; sy < 64; ++sy)
                    for (unsigned sx = 0; sx < 64; ++sx) {
                        const double value = side(x + (sx + 0.5) / 64, y + (sy + 0.5) / 64);
                        inside += value > 0 ? 1 : value == 0 ? 0.5 : 0;
                    }
                result.coverage[i] = 255.0 * inside / (64 * 64);
            }
        }
    return result;
}

// Retain only the retired grayscale five-tap kernel as a quality baseline.
// This intentionally does not duplicate the replacement endpoint search.
std::vector<Pixel> five_tap_reference(unsigned width, unsigned height, const std::vector<Pixel>& source) {
    auto result = source;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            const size_t i = y * width + x;
            const double center = source[i][0];
            const double north = source[y ? i - width : i][0], south = source[y + 1 < height ? i + width : i][0];
            const double west = source[x ? i - 1 : i][0], east = source[x + 1 < width ? i + 1 : i][0];
            const double low = std::min({center, north, south, east, west});
            const double high = std::max({center, north, south, east, west});
            const double contrast = high - low, threshold = std::max(255 * 0.0312, high * 0.125);
            if (contrast < threshold) continue;
            const bool horizontal = std::abs(north + south - 2 * center) >= std::abs(east + west - 2 * center);
            const double pair = horizontal ? (north + south) * 0.5 : (east + west) * 0.5;
            const double blend = std::clamp((contrast - threshold) / std::max(contrast, 0.00001), 0.0, 1.0) * 0.4;
            const auto level = static_cast<unsigned char>(std::lround(center + (pair - center) * blend));
            result[i][0] = result[i][1] = result[i][2] = level;
        }
    return result;
}

double coverage_error(unsigned width, unsigned height, const std::vector<Pixel>& image,
                      const std::vector<double>& coverage) {
    double squared = 0;
    unsigned count = 0;
    // Endpoints outside the image are not observable; score the interior edge.
    for (unsigned y = 12; y + 12 < height; ++y)
        for (unsigned x = 12; x + 12 < width; ++x) {
            const size_t i = y * width + x;
            const double error = image[i][0] - coverage[i];
            squared += error * error; ++count;
        }
    return squared / count;
}
} // namespace

int main() {
    auto instance = wgpuCreateInstance(nullptr);
    if (!instance) return 77;
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
#ifdef _WIN32
    options.backendType = WGPUBackendType_D3D12;
#elif defined(__APPLE__)
    options.backendType = WGPUBackendType_Metal;
#else
    options.backendType = WGPUBackendType_Vulkan;
#endif
    WGPURequestAdapterCallbackInfo ai = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    ai.mode = WGPUCallbackMode_WaitAnyOnly;
    ai.userdata1 = &adapter;
    ai.callback = [](WGPURequestAdapterStatus status, WGPUAdapter value, WGPUStringView,
                      void* user, void*) {
        if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(user) = value;
    };
    if (!wait(instance, wgpuInstanceRequestAdapter(instance, &options, ai)) || !adapter) {
        wgpuInstanceRelease(instance);
        return 77;
    }
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
    wgpuAdapterGetInfo(adapter, &info);
    std::printf("Adapter: %.*s; backend=%u; type=%u\n", int(info.device.length), info.device.data,
                unsigned(info.backendType), unsigned(info.adapterType));
    wgpuAdapterInfoFreeMembers(info);
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo di = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    di.mode = WGPUCallbackMode_WaitAnyOnly;
    di.userdata1 = &device;
    di.callback = [](WGPURequestDeviceStatus status, WGPUDevice value, WGPUStringView message,
                      void* user, void*) {
        if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(user) = value;
        else std::fprintf(stderr, "Device request failed: %.*s\n", int(message.length), message.data);
    };
    if (!wait(instance, wgpuAdapterRequestDevice(adapter, nullptr, di)) || !device) {
        wgpuAdapterRelease(adapter);
        wgpuInstanceRelease(instance);
        return 1;
    }
    bool ok = true;
    unsigned cases = 0;
    for (const auto size : {std::array<unsigned, 2>{1, 1}, {1, 17}, {19, 1}, {17, 13}, {65, 31}}) {
        const auto [width, height] = size;
        for (int pattern = 0; pattern < 5; ++pattern) {
            std::vector<Pixel> source(width * height);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) {
                    auto& pixel = source[y * width + x];
                    const unsigned char value = pattern == 0 ? 87 : pattern == 1 ?
                        static_cast<unsigned char>(100 + ((x + y) % 3)) :
                        static_cast<unsigned char>((x + y >= (width + height) / 2) ? 255 : 0);
                    const unsigned char level = pattern == 4 ? (100 + 8 * ((x + y) % 2)) :
                        pattern == 3 ? (value ? 120 : 80) : value;
                    pixel = {level, level, level, static_cast<unsigned char>((x * 37 + y * 19) % 256)};
                    if (pattern == 0) { pixel[1] = 129; pixel[2] = 201; }
                }
            const auto baseline = render(instance, device, "fs_main", width, height, source);
            const auto aa = render(instance, device, "fs_fxaa", width, height, source);
            const auto detail_zero = render(instance, device, "fs_fxaa_detail", width, height, source);
            const auto detail_active = render(instance, device, "fs_fxaa_detail", width, height, source, 1.0f);
            bool valid = detail_active.size() == source.size() && baseline == source && aa.size() == source.size() && detail_zero == aa;
            unsigned changed = 0;
            if (aa.size() == source.size()) {
                for (size_t i = 0; i < source.size(); ++i) {
                    valid &= aa[i][3] == source[i][3];
                    if (detail_active.size() == source.size())
                        valid &= detail_active[i][3] == source[i][3];
                    changed += aa[i] != source[i];
                    // The specified 40% blend cannot move a channel by more than 102/255.
                    for (int channel = 0; channel < 3; ++channel)
                        valid &= std::abs(int(aa[i][channel]) - int(source[i][channel])) <= 102;
                    // Uniform neighborhoods must stay untouched, even in an image with edges.
                    const unsigned x = unsigned(i) % width, y = unsigned(i) / width;
                    const size_t neighbors[]{y ? i - width : i, y + 1 < height ? i + width : i,
                                              x ? i - 1 : i, x + 1 < width ? i + 1 : i};
                    bool flat = true;
                    for (auto neighbor : neighbors)
                        for (int channel = 0; channel < 3; ++channel)
                            flat &= source[neighbor][channel] == source[i][channel];
                    if (flat) valid &= aa[i] == source[i];
                }
            }
            if (pattern == 3) valid &= detail_active == aa;
            if (pattern == 4 && source.size() > 1) valid &= detail_active != aa;
            if (pattern < 2 || pattern == 4 || width == 1 || height == 1) valid &= aa == source;
            else valid &= changed > 0;
            std::printf("%ux%u pattern=%d changed=%u: %s\n", width, height, pattern, changed,
                        valid ? "PASS" : "FAIL");
            if (!valid) {
                std::fprintf(stderr, "Failure diagnostics: %ux%u pattern=%d\n", width, height, pattern);
                report_difference("neutral identity", width, source, baseline);
                report_difference("zero-detail parity", width, aa, detail_zero);
                if (pattern < 2 || pattern == 4 || width == 1 || height == 1)
                    report_difference("AA identity", width, source, aa);
                if (pattern == 3)
                    report_difference("edge/detail parity", width, aa, detail_active);
            }
            ok &= valid;
            ++cases;
        }
    }
    std::printf("%u pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
    {
        constexpr unsigned width = 65, height = 49;
        for (bool horizontal : {false, true}) {
            std::vector<Pixel> source(width * height);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) {
                    const unsigned char level = (horizontal ? y >= height / 2 : x >= width / 2) ? 255 : 0;
                    source[y * width + x] = {level,level,level,static_cast<unsigned char>((x * 37 + y * 19) % 256)};
                }
            const auto actual = render(instance, device, "fs_fxaa", width, height, source);
            const bool valid = actual == source;
            std::printf("Axis-aligned %s step exact identity: %s\n", horizontal ? "horizontal" : "vertical", valid ? "PASS" : "FAIL");
            if (!valid) report_difference("axis identity", width, source, actual);
            ok &= valid;
        }
    }
    {
        constexpr unsigned size = 33;
        std::vector<Pixel> source(size * size), transposed(size * size);
        for (unsigned y = 0; y < size; ++y)
            for (unsigned x = 0; x < size; ++x) {
                const unsigned char level = 2 * y >= x ? 255 : 0;
                source[y * size + x] = {level,level,level,static_cast<unsigned char>((x * 37 + y * 19) % 256)};
            }
        for (unsigned y = 0; y < size; ++y)
            for (unsigned x = 0; x < size; ++x) transposed[y * size + x] = source[x * size + y];
        const auto actual = render(instance, device, "fs_fxaa", size, size, source);
        const auto actual_transposed = render(instance, device, "fs_fxaa", size, size, transposed);
        std::vector<Pixel> expected_transposed;
        bool valid = actual.size() == source.size() && actual_transposed.size() == source.size();
        if (valid) {
            expected_transposed.resize(source.size());
            for (unsigned y = 0; y < size; ++y)
                for (unsigned x = 0; x < size; ++x) {
                    expected_transposed[y * size + x] = actual[x * size + y];
                    valid &= actual[y * size + x][3] == source[y * size + x][3];
                }
            valid &= actual_transposed == expected_transposed;
        }
        std::printf("Shallow monotonic edge exact transpose, including borders and alpha: %s\n", valid ? "PASS" : "FAIL");
        if (!valid) report_difference("AA transpose", size, expected_transposed, actual_transposed);
        ok &= valid;
        auto complemented = source;
        for (auto& pixel : complemented)
            for (unsigned c = 0; c < 3; ++c) pixel[c] = 255 - pixel[c];
        const auto actual_complement = render(instance, device, "fs_fxaa", size, size, complemented);
        valid = actual.size() == source.size() && actual_complement.size() == source.size();
        if (valid) for (size_t i = 0; i < actual.size(); ++i) {
            valid &= actual_complement[i][3] == source[i][3];
            for (unsigned c = 0; c < 3; ++c)
                valid &= std::abs(int(actual[i][c]) + int(actual_complement[i][c]) - 255) <= 1;
        }
        std::printf("Binary edge brightness complement symmetry within one code: %s\n", valid ? "PASS" : "FAIL");
        ok &= valid;
    }
    {
        constexpr unsigned width = 65, height = 65;
        std::array<double, 2> source_error{}, old_error{}, actual_error{};
        for (double slope : {1.0, -1.0, 0.25, -0.25}) {
            for (bool transpose : {false, true}) {
                if (std::abs(slope) == 1 && transpose) continue;
                const auto plane = half_plane(width, height, slope, transpose);
                const auto actual = render(instance, device, "fs_fxaa", width, height, plane.source);
                bool valid = actual.size() == plane.source.size();
                const auto old = five_tap_reference(width, height, plane.source);
                const double source_mse = coverage_error(width, height, plane.source, plane.coverage);
                const double old_mse = coverage_error(width, height, old, plane.coverage);
                double actual_mse = std::numeric_limits<double>::infinity();
                if (valid) {
                    actual_mse = coverage_error(width, height, actual, plane.coverage);
                    for (size_t i = 0; i < actual.size(); ++i) {
                        valid &= actual[i][3] == plane.source[i][3];
                        for (unsigned c = 0; c < 3; ++c)
                            valid &= std::abs(int(actual[i][c]) - int(plane.source[i][c])) <= 102;
                    }
                    valid &= actual_mse < source_mse;
                }
                const unsigned group = std::abs(slope) == 1 ? 0 : 1;
                source_error[group] += source_mse; old_error[group] += old_mse; actual_error[group] += actual_mse;
                std::printf("Half-plane slope=%g transpose=%u MSE source=%.6f old=%.6f new=%.6f: %s\n",
                            slope, unsigned(transpose), source_mse, old_mse, actual_mse, valid ? "PASS" : "FAIL");
                ok &= valid;
                if (slope == 0.25 && !transpose) {
                    const auto water = render(instance, device, "fs_absorption_fxaa", width, height, plane.source,
                        0, -1, 1, 0, 0, WGPUTextureFormat_RGBA8Unorm, 0, 0, false);
                    const bool parity = water == actual;
                    std::printf("Inactive water and ordinary AA exact parity: %s\n", parity ? "PASS" : "FAIL");
                    if (!parity) report_difference("water AA parity", width, actual, water);
                    ok &= parity;
                }
            }
        }
        for (unsigned group = 0; group < 2; ++group) {
            const bool valid = actual_error[group] < source_error[group] * 0.98 &&
                               actual_error[group] < old_error[group] * 0.98;
            std::printf("%s coverage improvement >=2%% over source and retired kernel: %s\n",
                        group ? "Shallow slopes" : "Diagonals", valid ? "PASS" : "FAIL");
            ok &= valid;
        }
        auto colors = half_plane(width, height, 0.25, false).source;
        for (auto& pixel : colors) {
            const auto alpha = pixel[3];
            pixel = pixel[0] ? Pixel{240,16,32,alpha} : Pixel{8,208,176,alpha};
        }
        const auto actual = render(instance, device, "fs_fxaa", width, height, colors);
        bool valid = actual.size() == colors.size();
        unsigned changed = 0;
        if (valid) for (size_t i = 0; i < actual.size(); ++i) {
            valid &= actual[i][3] == colors[i][3] && actual[i][0] >= 8 && actual[i][0] <= 240 &&
                     actual[i][1] >= 16 && actual[i][1] <= 208 && actual[i][2] >= 32 && actual[i][2] <= 176;
            const double red = (240.0 - actual[i][0]) / 232;
            const double green = (actual[i][1] - 16.0) / 192;
            const double blue = (actual[i][2] - 32.0) / 144;
            valid &= std::abs(red - green) <= 0.012 && std::abs(red - blue) <= 0.012;
            changed += actual[i] != colors[i];
        }
        valid &= changed > 0;
        std::printf("Saturated edge retains channel bounds, hue and alpha: %s\n", valid ? "PASS" : "FAIL");
        ok &= valid;
    }
    {
        constexpr unsigned width = 65, height = 49;
        for (unsigned pattern = 0; pattern < 8; ++pattern) {
            std::vector<Pixel> source(width * height);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) {
                    const bool light = pattern == 0 ? x == width / 2 : pattern == 1 ? y == height / 2 :
                                       pattern == 2 ? (x + y) % 2 != 0 : pattern == 3 ? x == width / 2 && y == height / 2 :
                                       pattern == 4 ? y == x : pattern == 5 ? y + x == width - 1 :
                                       pattern == 6 ? y != x : y + x != width - 1;
                    const unsigned char level = light ? 255 : 0;
                    source[y * width + x] = {level,level,level,static_cast<unsigned char>((x * 37 + y * 19) % 256)};
                }
            const auto actual = render(instance, device, "fs_fxaa", width, height, source);
            const bool valid = actual == source;
            const char* names[]{"vertical one-pixel ridge", "horizontal one-pixel ridge", "checkerboard", "isolated pixel",
                                "descending diagonal one-pixel ridge", "ascending diagonal one-pixel ridge",
                                "descending diagonal one-pixel valley", "ascending diagonal one-pixel valley"};
            std::printf("Preserve %s exactly: %s\n", names[pattern], valid ? "PASS" : "FAIL");
            if (!valid) report_difference("fine-detail identity", width, source, actual);
            ok &= valid;
        }
    }
    // Exercise all 256 code values, independent RGB channels, boundaries,
    // padded rows, degenerate dimensions, and resource recreation per draw.
    for (auto size : {std::array<unsigned, 2>{1, 1}, {1, 257}, {257, 1}, {257, 3}}) {
        const auto [w, h] = size;
        std::vector<Pixel> source(w * h);
        for (unsigned i = 0; i < source.size(); ++i)
            source[i] = {static_cast<unsigned char>(i % 256),
                         static_cast<unsigned char>((i * 73) % 256),
                         static_cast<unsigned char>(255 - i % 256),
                         static_cast<unsigned char>((i * 19) % 256)};
        for (auto entry : {"fs_main", "fs_detail", "fs_fxaa", "fs_fxaa_detail",
                           "fs_absorption", "fs_absorption_detail", "fs_absorption_fxaa", "fs_absorption_fxaa_detail"}) {
          for (float gain : {0.25f, 1.0f, 4.0f}) {
            auto baseline = render(instance, device, entry, w, h, source, 0.5f, -1, gain);
            for (float intensity : {0.0f, 0.5f, 1.0f}) {
                auto identity = render(instance, device, entry, w, h, source, 0.5f, intensity, gain);
                bool valid = baseline.size() == source.size() && identity.size() == source.size();
                if (valid) for (unsigned i = 0; i < source.size(); ++i) {
                    valid &= identity[i][3] == source[i][3];
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= std::abs(int(identity[i][c]) - int(baseline[i][c])) <= (intensity == 0 ? 0 : 1);
                }
                std::printf("Identity LUT %ux%u %s intensity=%.1f: %s\n", w, h, entry, intensity, valid ? "PASS" : "FAIL");
                if (!valid) report_difference("identity LUT", w, baseline, identity);
                ok &= valid;
            }
          }
        }
    }
    const std::vector<Pixel> boundaries{{0,0,0,0}, {255,255,255,255}, {255,0,0,1},
        {0,255,0,2}, {0,0,255,3}, {255,255,0,4}, {255,0,255,5}, {0,255,255,6},
        {1,2,3,7}, {3,2,1,8}, {127,128,129,9}};
    for (auto format : {WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_BGRA8Unorm}) {
        const auto result = render(instance, device, "fs_main", unsigned(boundaries.size()), 1,
                                   boundaries, 0, 1, 1, 0, 0, format);
        const bool valid = result == boundaries;
        std::printf("Identity primaries, secondaries, low codes, format=%u: %s\n",
                    unsigned(format), valid ? "PASS" : "FAIL");
        ok &= valid;
    }
    const bool half_ok = midnafx::lut::half(0.0f) == 0 && midnafx::lut::half(1.0f) == 0x3c00 &&
        midnafx::lut::half(0x1p-25f) == 0 && midnafx::lut::half(0x1.8p-25f) == 1 &&
        midnafx::lut::half(0x1p-24f) == 1 && midnafx::lut::half(0x1.ffcp-15f) == 0x400;
    std::printf("Half conversion endpoints and subnormal rounding: %s\n", half_ok ? "PASS" : "FAIL");
    ok &= half_ok;
    std::vector<Pixel> ramp(256);
    for (unsigned i = 0; i < ramp.size(); ++i)
        ramp[i] = {static_cast<unsigned char>(i), static_cast<unsigned char>(i),
                   static_cast<unsigned char>(i), static_cast<unsigned char>(255 - i)};
    const auto natural = render(instance, device, "fs_main", 256, 1, ramp, 0, 1, 1, 1, 0);
    const auto twilight = render(instance, device, "fs_main", 256, 1, ramp, 0, 1, 1, 1, 1);
    const auto midpoint = render(instance, device, "fs_main", 256, 1, ramp, 0, 1, 1, 1, 0.5f);
    const auto bypass = render(instance, device, "fs_main", 256, 1, ramp, 0, 0, 1, 1, 1);
    bool creative_ok = natural.size() == ramp.size() && twilight.size() == ramp.size() &&
                       midpoint.size() == ramp.size() && bypass == ramp;
    if (creative_ok) {
        creative_ok &= natural != ramp && twilight != natural;
        for (unsigned i = 0; i < ramp.size(); ++i) {
            creative_ok &= natural[i][3] == ramp[i][3] && twilight[i][3] == ramp[i][3] && midpoint[i][3] == ramp[i][3];
            for (unsigned c = 0; c < 3; ++c) {
                creative_ok &= std::abs(2 * int(midpoint[i][c]) - int(natural[i][c]) - int(twilight[i][c])) <= 2;
                if (i) creative_ok &= natural[i][c] >= natural[i - 1][c] && twilight[i][c] >= twilight[i - 1][c];
                if (i == 0 || i == 255) creative_ok &= natural[i][c] == i && twilight[i][c] == i;
            }
        }
    }
    std::printf("Creative endpoints, monotonic ramp, Twilight midpoint, zero-strength bypass: %s\n", creative_ok ? "PASS" : "FAIL");
    ok &= creative_ok;
    // Check creative controls through every production entry point, using
    // independent channels so channel-order mistakes cannot hide in a gray ramp.
    std::vector<Pixel> colors(17 * 3);
    for (unsigned i = 0; i < colors.size(); ++i)
        colors[i] = {static_cast<unsigned char>((i * 37 + 17) % 256),
                     static_cast<unsigned char>((i * 73 + 83) % 256),
                     static_cast<unsigned char>((i * 19 + 151) % 256),
                     static_cast<unsigned char>((i * 29) % 256)};
    for (auto format : {WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_BGRA8Unorm}) {
        for (auto entry : {"fs_main", "fs_detail", "fs_fxaa", "fs_fxaa_detail",
                           "fs_absorption", "fs_absorption_detail", "fs_absorption_fxaa", "fs_absorption_fxaa_detail"}) {
            const auto base = render(instance, device, entry, 17, 3, colors, 0.5f, -1, 1, 1, 0, format);
            const auto zero = render(instance, device, entry, 17, 3, colors, 0.5f, 0, 1, 1, 1, format);
            const auto natural_full = render(instance, device, entry, 17, 3, colors, 0.5f, 1, 1, 1, 0, format);
            const auto twilight_full = render(instance, device, entry, 17, 3, colors, 0.5f, 1, 1, 1, 1, format);
            const auto blend_half = render(instance, device, entry, 17, 3, colors, 0.5f, 1, 1, 1, 0.5f, format);
            const auto intensity_half = render(instance, device, entry, 17, 3, colors, 0.5f, 0.5f, 1, 1, 1, format);
            bool valid = base.size() == colors.size() && zero == base &&
                natural_full.size() == colors.size() && twilight_full.size() == colors.size() &&
                blend_half.size() == colors.size() && intensity_half.size() == colors.size();
            if (valid) {
                valid &= natural_full != base && twilight_full != natural_full;
                for (unsigned i = 0; i < colors.size(); ++i) {
                    for (const auto* result : {&base, &zero, &natural_full, &twilight_full, &blend_half, &intensity_half})
                        valid &= (*result)[i][3] == colors[i][3];
                    for (unsigned c = 0; c < 3; ++c) {
                        // Compare independently quantized outputs, allowing two
                        // code values for the doubled midpoint's rounding.
                        valid &= std::abs(2 * int(blend_half[i][c]) - int(natural_full[i][c]) - int(twilight_full[i][c])) <= 2;
                        valid &= std::abs(2 * int(intensity_half[i][c]) - int(base[i][c]) - int(twilight_full[i][c])) <= 2;
                    }
                }
            }
            std::printf("Creative controls %s format=%u: %s\n", entry, unsigned(format), valid ? "PASS" : "FAIL");
            ok &= valid;
        }
    }
    std::vector<MeterStep> meter_steps;
    const auto append_scene = [&](Pixel color, unsigned frames, bool reset = true) {
        const size_t first = meter_steps.size();
        for (unsigned i = 0; i < frames; ++i)
            meter_steps.push_back({std::vector<Pixel>(256, color),
                                   reset && i == 0 ? 0.0f : 0.1f,
                                   reset && i == 0 ? 1u : 0u});
        return first;
    };
    const auto dark = append_scene({32, 32, 32, 17}, 61);
    const auto bright_transition = append_scene({255, 255, 255, 29}, 31, false);
    const auto bright = append_scene({255, 255, 255, 0}, 61);
    const auto middle = append_scene({82, 82, 82, 255}, 61);
    const auto night = append_scene({12, 12, 12, 85}, 61);
    const auto red = append_scene({128, 0, 0, 7}, 1);
    const auto blue = append_scene({0, 0, 128, 99}, 1);
    const auto alpha_variant = append_scene({32, 32, 32, 255}, 2);
    const auto before_fade = append_scene({32, 32, 32, 0}, 31);
    const auto fade = append_scene({4, 4, 4, 123}, 1, false);
    const auto black = append_scene({0, 0, 0, 255}, 1, false);
    // Invalid dark history must restart from neutral, even without a reset bit.
    const size_t recovery = meter_steps.size();
    meter_steps.push_back({std::vector<Pixel>(256, Pixel{32, 32, 32, 123}), 0, 0});
    append_scene({32, 32, 32, 123}, 1, false);
    const auto unframed = append_scene({128, 128, 128, 51}, 1);
    const size_t framed = meter_steps.size();
    auto bars = std::vector<Pixel>(256, Pixel{128, 128, 128, 51});
    for (unsigned y = 0; y < 16; ++y)
        if (y < 4 || y >= 12)
            for (unsigned x = 0; x < 16; ++x) bars[y * 16 + x] = {0, 0, 0, 255};
    meter_steps.push_back({std::move(bars), 0, 1});
    const auto metered = meter(instance, device, meter_steps);
    bool meter_ok = metered.size() == meter_steps.size();
    if (meter_ok) {
        for (const auto& value : metered)
            meter_ok &= std::isfinite(value[0]) && std::isfinite(value[1]) &&
                        value[0] >= -0.5f && value[0] <= 0.75f && value[3] == 1;
        for (auto first : {dark, bright, middle, night, red, blue, alpha_variant, before_fade, unframed})
            meter_ok &= metered[first][0] == 0 && metered[first][2] == 1;
        meter_ok &= metered[dark + 60][0] > 0.70f &&
                    metered[bright + 60][0] < -0.48f &&
                    std::abs(metered[middle + 60][0]) < 0.02f;
        for (unsigned i = 1; i < 61; ++i) {
            meter_ok &= metered[dark + i][0] >= metered[dark + i - 1][0];
            meter_ok &= metered[bright + i][0] <= metered[bright + i - 1][0];
        }
        for (unsigned i = 1; i < 31; ++i)
            meter_ok &= metered[bright_transition + i][0] <= metered[bright_transition + i - 1][0];
        meter_ok &= metered[bright_transition][0] < metered[dark + 60][0] &&
                    metered[bright_transition + 30][0] < -0.48f;
        // Compare fractions of the two clamped targets to separate speed from range.
        meter_ok &= -metered[bright + 1][0] / 0.5f > metered[dark + 1][0] / 0.75f;
        meter_ok &= std::abs(metered[red][1] - (128.0f / 255 * 0.2126f)) < 0.0003f &&
                    std::abs(metered[blue][1] - (128.0f / 255 * 0.0722f)) < 0.0001f;
        meter_ok &= metered[alpha_variant + 1] == metered[dark + 1];
        meter_ok &= metered[before_fade + 30][0] > 0.5f &&
                    metered[fade][0] == 0 && metered[fade][2] == 0 &&
                    metered[black][0] == 0 && metered[black][2] == 0 &&
                    metered[recovery][0] == 0 && metered[recovery][2] == 1 &&
                    metered[recovery + 1][0] > 0;
        meter_ok &= std::abs(metered[framed][1] - metered[unframed][1]) < 0.0005f;
        const auto night_output = render(instance, device, "fs_main", 1, 1, {{12, 12, 12, 85}},
            0, -1, 1, 0, 0, WGPUTextureFormat_RGBA8Unorm, metered[night + 60][0]);
        const auto day_output = render(instance, device, "fs_main", 1, 1, {{82, 82, 82, 255}},
            0, -1, 1, 0, 0, WGPUTextureFormat_RGBA8Unorm, metered[middle + 60][0]);
        meter_ok &= night_output.size() == 1 && day_output.size() == 1;
        if (night_output.size() == 1 && day_output.size() == 1)
            meter_ok &= night_output[0][0] < day_output[0][0] &&
                        night_output[0][3] == 85 && day_output[0][3] == 255;
    }
    std::printf("Exposure meter: convergence, Rec709, temporal history, reset, fades, bars, alpha: %s\n",
                meter_ok ? "PASS" : "FAIL");
    if (!meter_ok && metered.size() == meter_steps.size())
        for (auto i : {dark, dark + 1, dark + 60, bright_transition, bright_transition + 30,
                       bright + 1, bright + 60, middle + 60, red, blue, fade, black, recovery, framed})
            std::fprintf(stderr, "Meter step %zu: stops=%g mean=%g valid=%g alpha=%g\n",
                         i, metered[i][0], metered[i][1], metered[i][2], metered[i][3]);
    ok &= meter_ok;

    std::vector<Pixel> exposure_source(17 * 3);
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < 17; ++x)
            exposure_source[y * 17 + x] = x < 5 ? Pixel{0, 0, 0, static_cast<unsigned char>(x + y)} :
                x < 12 ? Pixel{64, 96, 128, static_cast<unsigned char>(x * 13 + y)} :
                         Pixel{255, 255, 255, static_cast<unsigned char>(x * 13 + y)};
    for (const auto* entry : {"fs_main", "fs_detail", "fs_fxaa", "fs_fxaa_detail",
                             "fs_absorption", "fs_absorption_detail", "fs_absorption_fxaa", "fs_absorption_fxaa_detail"}) {
        const float look = std::strstr(entry, "detail") ? 1.0f : -1.0f;
        // Exposure follows water absorption and filtering, then precedes the LUT.
        // Capture the ungraded result rather than exposing the original water input.
        const auto reference = render(instance, device, entry, 17, 3, exposure_source, 0.5f);
        const auto neutral = look < 0 ? reference :
            render(instance, device, entry, 17, 3, exposure_source, 0.5f, look, 1, 1);
        for (float stops : {-0.5f, 0.75f}) {
            auto pre_exposed = reference;
            const float gain = std::exp2(stops);
            for (auto& pixel : pre_exposed) {
                const float peak = float(std::max(std::max(pixel[0], pixel[1]), pixel[2])) / 255;
                for (unsigned c = 0; c < 3; ++c)
                    pixel[c] = static_cast<unsigned char>(std::lround(
                        float(pixel[c]) * gain / (1 + std::max(gain - 1, 0.0f) * peak)));
            }
            const auto expected = render(instance, device, "fs_main", 17, 3, pre_exposed, 0, look, 1, 1);
            const auto actual = render(instance, device, entry, 17, 3, exposure_source,
                                       0.5f, look, 1, 1, 0, WGPUTextureFormat_RGBA8Unorm, stops);
            bool valid = actual.size() == exposure_source.size() && expected.size() == actual.size() &&
                         neutral.size() == actual.size() && actual != neutral;
            if (valid) {
                for (unsigned i = 0; i < actual.size(); ++i) valid &= actual[i][3] == exposure_source[i][3];
                // Sample flat interiors; filtering and nonlinear exposure need not commute at edges.
                for (unsigned y = 0; y < 3; ++y)
                    for (unsigned x : {0u, 8u, 16u})
                        for (unsigned c = 0; c < 3; ++c) {
                            valid &= std::abs(int(actual[y * 17 + x][c]) - int(expected[y * 17 + x][c])) <= 2;
                            if (!std::strncmp(entry, "fs_absorption", 13)) continue;
                            if (x == 0 || (x == 16 && stops > 0))
                                valid &= actual[y * 17 + x][c] == (x == 0 ? 0 : 255);
                        }
            }
            std::printf("Exposure composite %s LUT=%s stops=%.2f: %s\n", entry,
                        look >= 0 ? "on" : "off", stops, valid ? "PASS" : "FAIL");
            if (!valid) report_difference("exposure composite", 17, expected, actual);
            ok &= valid;
        }
    }
    // A constant light field isolates composition order from the separate
    // extraction/blur checks. Water must receive glow after its absorption.
    const std::vector<Pixel> bloom_source(17 * 3, Pixel{82, 128, 200, 37});
    for (auto entry : {"fs_main", "fs_detail", "fs_fxaa", "fs_fxaa_detail",
                       "fs_absorption", "fs_absorption_detail", "fs_absorption_fxaa", "fs_absorption_fxaa_detail"}) {
        for (float look : {-1.0f, 1.0f}) {
            const auto base = render(instance, device, entry, 17, 3, bloom_source, 0.5f, look);
            const auto glow = render(instance, device, entry, 17, 3, bloom_source, 0.5f, look,
                1, 0, 0, WGPUTextureFormat_RGBA8Unorm, 0, 0.125f);
            bool valid = base.size() == bloom_source.size() && glow.size() == base.size() && glow != base;
            if (valid) for (size_t i = 0; i < base.size(); ++i) {
                valid &= glow[i][3] == bloom_source[i][3];
                for (unsigned c = 0; c < 3; ++c) {
                    const int expected = int(std::lround(base[i][c] + 0.125f * (255 - base[i][c])));
                    valid &= std::abs(int(glow[i][c]) - expected) <= 1;
                }
            }
            std::printf("Bloom composite %s LUT=%s: %s\n", entry, look >= 0 ? "identity" : "off", valid ? "PASS" : "FAIL");
            ok &= valid;
        }
    }
    {
        const auto draw_water = [&](unsigned width, unsigned height, const WaterScene& scene) {
            return render(instance, device, "fs_absorption", width, height, scene.pixels,
                0, -1, 1, 0, 0, WGPUTextureFormat_RGBA8Unorm, 0, 0, true, &scene.input);
        };
        for (auto size : {std::array<unsigned,2>{96,80}, {65,49}}) {
            const auto [width, height] = size;
            auto red = reflection_scene(width, height, Pixel{230,25,25,255});
            auto blue = reflection_scene(width, height, Pixel{25,25,230,255});
            auto red_off = red; red_off.input.parameters[69] = 0;
            auto blue_off = blue; blue_off.input.parameters[69] = 0;
            const auto red_on_pixels = draw_water(width, height, red);
            const auto blue_on_pixels = draw_water(width, height, blue);
            const auto red_off_pixels = draw_water(width, height, red_off);
            const auto blue_off_pixels = draw_water(width, height, blue_off);
            bool valid = reflection_alpha(red, red_on_pixels) && reflection_alpha(blue, blue_on_pixels) &&
                         reflection_alpha(red_off, red_off_pixels) && reflection_alpha(blue_off, blue_off_pixels);
            unsigned red_hits = 0, blue_hits = 0;
            if (valid) for (size_t i = 0; i < red.pixels.size(); ++i) {
                if (red.input.mask[i][3]) {
                    red_hits += red_on_pixels[i][0] > red_off_pixels[i][0] + 1 &&
                                red_on_pixels[i][0] > blue_on_pixels[i][0];
                    blue_hits += blue_on_pixels[i][2] > blue_off_pixels[i][2] + 1 &&
                                 blue_on_pixels[i][2] > red_on_pixels[i][2];
                    for (unsigned c = 0; c < 3; ++c) {
                        // A hit only replaces the old tint inside the bounded
                        // Fresnel mix; no channel may exceed that 45% envelope.
                        const int red_bound = int(std::ceil(0.45f * std::abs(float(red.input.behind_water[i][c]) -
                            red.input.parameters[64 + c] * 255))) + 1;
                        const int blue_bound = int(std::ceil(0.45f * std::abs(float(blue.input.behind_water[i][c]) -
                            blue.input.parameters[64 + c] * 255))) + 1;
                        valid &= std::abs(int(red_on_pixels[i][c]) - int(red_off_pixels[i][c])) <= red_bound;
                        valid &= std::abs(int(blue_on_pixels[i][c]) - int(blue_off_pixels[i][c])) <= blue_bound;
                    }
                } else {
                    valid &= red_on_pixels[i] == red.pixels[i] && blue_on_pixels[i] == blue.pixels[i];
                }
            }
            valid &= red_hits > 0 && blue_hits > 0;
            std::printf("Analytic pitched water reflects wall donor hue within Fresnel bounds %ux%u red=%u blue=%u: %s\n",
                        width, height, red_hits, blue_hits, valid ? "PASS" : "FAIL");
            ok &= valid;

            if (width == 96) {
                auto disabled_poison = red_off;
                for (auto& pixel : disabled_poison.input.behind_water) pixel = {0,255,0,255};
                const auto poisoned = draw_water(width, height, disabled_poison);
                auto zero = red; zero.input.parameters[68] = 0;
                const auto zero_pixels = draw_water(width, height, zero);
                auto inactive = red;
                for (auto& pixel : inactive.input.mask) pixel[3] = 0;
                const auto inactive_pixels = draw_water(width, height, inactive);
                valid = poisoned == red_off_pixels && zero_pixels == zero.pixels &&
                        inactive_pixels == inactive.pixels;
                std::printf("Disabled scene reflections, zero reflection strength, and inactive mask retain exact native paths: %s\n",
                            valid ? "PASS" : "FAIL");
                ok &= valid;

                auto miss = red;
                for (size_t i = 0; i < miss.pixels.size(); ++i)
                    if (!red.input.mask[i][3]) miss.input.scene_depth[i] = 1;
                auto miss_off = miss; miss_off.input.parameters[69] = 0;
                const auto miss_pixels = draw_water(width, height, miss);
                valid = reflection_alpha(miss, miss_pixels) && miss_pixels == draw_water(width, height, miss_off);
                std::printf("Missing visible wall depth retains the exact previous reflection tint: %s\n", valid ? "PASS" : "FAIL");
                ok &= valid;

                auto hidden_water = red;
                const float angle = 15 * std::acos(-1.0f) / 180;
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const size_t i = size_t(y) * width + x;
                        if (red.input.mask[i][3] || red.input.scene_depth[i] == 1) continue;
                        const float ray_y = 1 - 2 * (y + 0.5f) / height;
                        const float wall = -6 / (-std::sin(angle) * ray_y - std::cos(angle));
                        hidden_water.input.mask[i][3] = 255;
                        // This separation is smaller than the receiver-origin
                        // bias. Opaque wall donors must still remain eligible.
                        hidden_water.input.surface_depth[i] = reflection_depth(wall + 0.03f);
                    }
                const auto hidden_pixels = draw_water(width, height, hidden_water);
                valid = reflection_alpha(hidden_water, hidden_pixels) && hidden_pixels == red_on_pixels;
                std::printf("Opaque wall with a water mask just behind it retains reflected donor color: %s\n",
                            valid ? "PASS" : "FAIL");
                ok &= valid;
                auto visible_water = hidden_water;
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const size_t i = size_t(y) * width + x;
                        if (red.input.mask[i][3] || red.input.scene_depth[i] == 1) continue;
                        const float ray_y = 1 - 2 * (y + 0.5f) / height;
                        const float wall = -6 / (-std::sin(angle) * ray_y - std::cos(angle));
                        visible_water.input.surface_depth[i] = reflection_depth(wall - 0.03f);
                    }
                auto visible_water_off = visible_water; visible_water_off.input.parameters[69] = 0;
                const auto visible_pixels = draw_water(width, height, visible_water);
                const auto visible_off_pixels = draw_water(width, height, visible_water_off);
                valid = reflection_alpha(visible_water, visible_pixels) && reflection_alpha(visible_water_off, visible_off_pixels);
                if (valid) for (size_t i = 0; i < red.pixels.size(); ++i)
                    if (red.input.mask[i][3]) valid &= visible_pixels[i] == visible_off_pixels[i];
                std::printf("Visible water in front of the wall is rejected as a reflection donor: %s\n", valid ? "PASS" : "FAIL");
                ok &= valid;

                auto below = reflection_scene(width, height, Pixel{230,25,25,255}, false, -1, -15);
                auto below_off = below; below_off.input.parameters[69] = 0;
                const auto below_pixels = draw_water(width, height, below);
                valid = reflection_alpha(below, below_pixels) && below_pixels == draw_water(width, height, below_off);
                std::printf("Underwater viewing direction retains the exact old tint fallback: %s\n", valid ? "PASS" : "FAIL");
                ok &= valid;

                auto foreground = red;
                for (size_t i = 0; i < red.pixels.size(); ++i)
                    if (red.input.mask[i][3]) foreground.input.scene_depth[i] = reflection_depth((1 + red.water_distance[i]) * 0.5f);
                const auto foreground_pixels = draw_water(width, height, foreground);
                valid = reflection_alpha(foreground, foreground_pixels) && foreground_pixels == foreground.pixels;
                std::printf("Opaque foreground in front of masked water retains original scene pixels: %s\n", valid ? "PASS" : "FAIL");
                ok &= valid;

                auto clipped = reflection_scene(width, height, Pixel{230,25,25,255}, false, 1, 60);
                auto clipped_off = clipped; clipped_off.input.parameters[69] = 0;
                const auto clipped_pixels = draw_water(width, height, clipped);
                const auto clipped_off_pixels = draw_water(width, height, clipped_off);
                valid = reflection_alpha(clipped, clipped_pixels) && reflection_alpha(clipped_off, clipped_off_pixels);
                unsigned clipped_receivers = 0;
                if (valid) for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const size_t i = size_t(y) * width + x;
                        if (!clipped.input.mask[i][3]) continue;
                        const float sine = std::sin(std::acos(-1.0f) / 3), cosine = 0.5f;
                        const float ray_y = 1 - 2 * (y + 0.5f) / height;
                        const float dy = cosine * ray_y - sine, dz = -sine * ray_y - cosine;
                        const float water_z = clipped.water_distance[i] * dz;
                        const float hit_t = (-6 - water_z) / dz;
                        const float hit_y = -dy * hit_t;
                        const float hit_view_z = sine * (hit_y - 1) - 6 * cosine;
                        if (hit_t > 0 && hit_view_z >= -1) {
                            ++clipped_receivers;
                            valid &= clipped_pixels[i] == clipped_off_pixels[i];
                        }
                    }
                valid &= clipped_receivers > 0;
                std::printf("Analytic reflected hits clipped by the near plane retain tint fallback receivers=%u: %s\n",
                            clipped_receivers, valid ? "PASS" : "FAIL");
                ok &= valid;

                const auto reversed = reflection_scene(width, height, Pixel{230,25,25,255}, true);
                const auto reversed_pixels = draw_water(width, height, reversed);
                valid = reflection_alpha(reversed, reversed_pixels) && reversed_pixels.size() == red_on_pixels.size();
                if (valid) for (size_t i = 0; i < red_on_pixels.size(); ++i)
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= std::abs(int(reversed_pixels[i][c]) - int(red_on_pixels[i][c])) <= 1;
                std::printf("Matched reversed-Z projection retains wall reflection and source alpha: %s\n", valid ? "PASS" : "FAIL");
                ok &= valid;
            }
        }
        bool valid = true;
        for (auto size : {std::array<unsigned,2>{1,1}, {3,5}, {17,13}}) {
            const auto [width, height] = size;
            const auto scene = reflection_scene(width, height, Pixel{230,25,25,255});
            valid &= reflection_alpha(scene, draw_water(width, height, scene));
        }
        std::printf("Small and odd water reflection targets stay bounded and preserve alpha: %s\n", valid ? "PASS" : "FAIL");
        ok &= valid;
    }
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
