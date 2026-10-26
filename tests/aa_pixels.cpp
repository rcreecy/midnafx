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
#include <thread>
#include <string>
#include <utility>
#include <vector>

namespace {
using Pixel = std::array<unsigned char, 4>;

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
                          float exposure_stops = 0.0f) {
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

    // Production Grading layout, with neutral tone controls and selectable detail.
    const bool water = std::strncmp(entry, "fs_absorption", 13) == 0;
    const std::array<float, 12> neutral{gain, gain, gain, 0, 1, 1, 1, 0, detail_strength, 1, 0, 0};
    std::vector<float> parameters(water ? 96 : 12);
    std::copy(neutral.begin(), neutral.end(), parameters.end() - 12);
    std::array<WGPUTexture, 3> water_textures{};
    std::array<WGPUTextureView, 3> water_views{};
    if (water) {
        for (unsigned matrix = 0; matrix < 3; ++matrix)
            for (unsigned diagonal = 0; diagonal < 4; ++diagonal)
                parameters[matrix * 16 + diagonal * 5] = 1;
        parameters[48] = 2; parameters[49] = 1; parameters[50] = 0.65f;
        parameters[52] = 0.2f; parameters[53] = 0.3f; parameters[54] = 0.4f;
        parameters[56] = 0.1f; parameters[57] = 0.2f; parameters[58] = 0.3f;
        parameters[61] = 1;
        auto texture_desc = td;
        texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        for (unsigned i = 0; i < 3; ++i) {
            water_textures[i] = wgpuDeviceCreateTexture(device, &texture_desc);
            water_views[i] = wgpuTextureCreateView(water_textures[i], nullptr);
            std::vector<Pixel> contents(pixels.size(), Pixel{static_cast<unsigned char>(i == 0 ? 192 : 64), 0, 0, 255});
            upload.texture = water_textures[i];
            wgpuQueueWriteTexture(queue, &upload, contents.data(), contents.size() * sizeof(Pixel), &upload_layout, &td.size);
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
    WGPUBindGroupEntry entries[11];
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
    if (water) { entries[count].binding = 5; entries[count++].textureView = source_view; }
    if (look_strength >= 0) {
        entries[count].binding = 6; entries[count++].textureView = lattice.view;
        entries[count].binding = 7; entries[count++].textureView = twilight_lattice.view;
        entries[count].binding = 8; entries[count++].sampler = look_sampler;
        entries[count].binding = 9; entries[count].buffer = look_uniform;
        entries[count++].size = sizeof(midnafx::lut::Controls);
    }
    entries[count].binding = 10;
    entries[count++].textureView = exposure_view;
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
    for (auto view : water_views) if (view) wgpuTextureViewRelease(view);
    for (auto texture : water_textures) if (texture) wgpuTextureRelease(texture);
    wgpuQueueRelease(queue);
    return output;
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
            if (pattern < 2 || pattern == 4 || source.size() == 1) valid &= aa == source;
            else valid &= changed > 0;
            std::printf("%ux%u pattern=%d changed=%u: %s\n", width, height, pattern, changed,
                        valid ? "PASS" : "FAIL");
            if (!valid) {
                std::fprintf(stderr, "Failure diagnostics: %ux%u pattern=%d\n", width, height, pattern);
                report_difference("neutral identity", width, source, baseline);
                report_difference("zero-detail parity", width, aa, detail_zero);
                if (pattern < 2 || pattern == 4 || source.size() == 1)
                    report_difference("AA identity", width, source, aa);
                if (pattern == 3)
                    report_difference("edge/detail parity", width, aa, detail_active);
            }
            ok &= valid;
            ++cases;
        }
    }
    std::printf("%u pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
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
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
