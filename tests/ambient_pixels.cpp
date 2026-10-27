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
#include <thread>
#include <utility>
#include <vector>

namespace {
using Pixel = std::array<unsigned char, 4>;
using Field = std::array<float, 4>;
struct Params {
    float view_from_proj[16]{};
    float radius = 2, strength = 0.8f, projection_y = 1, background_depth = 1;
    float fog_color[4]{};
    float fog_range[4]{};
    float proj_from_view[16]{};
    float light_position[4]{};
    float shadow_range[4]{};
};
static_assert(sizeof(Params) == 208);

bool wait(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!info.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &info, 0);
        if ((status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) ||
            std::chrono::steady_clock::now() > deadline) {
            std::fprintf(stderr, "Ambient GPU callback failed or timed out\n");
            std::exit(1);
        }
        if (!info.completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

float decode_half(std::uint16_t value) {
    const unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
    if (exponent == 31) return NAN;
    const float magnitude = exponent ? std::ldexp(float(1024 + fraction), int(exponent) - 25) :
                                       std::ldexp(float(fraction), -24);
    return value & 0x8000 ? -magnitude : magnitude;
}

// Right-handed perspective, near=1 and far=100, with WebGPU depth in [0,1].
float depth_at(float distance) { return 100.0f / 99 - (100.0f / 99) / distance; }
Params perspective(unsigned width, unsigned height) {
    Params params;
    params.view_from_proj[0] = float(width) / height;
    params.view_from_proj[5] = 1;
    params.view_from_proj[11] = -99.0f / 100;
    params.view_from_proj[14] = -1;
    params.view_from_proj[15] = 1;
    params.proj_from_view[0] = float(height) / width;
    params.proj_from_view[5] = 1;
    params.proj_from_view[10] = -100.0f / 99;
    params.proj_from_view[11] = -1;
    params.proj_from_view[14] = -100.0f / 99;
    return params;
}
std::vector<Pixel> scene(unsigned width, unsigned height) {
    std::vector<Pixel> result(width * height);
    for (unsigned i = 0; i < result.size(); ++i)
        result[i] = {120, 160, 200, static_cast<unsigned char>((i * 29 + 7) % 256)};
    return result;
}

struct Output {
    std::vector<Pixel> color;
    std::vector<Field> raw, filtered;
};
struct Fixture {
    WGPUInstance instance;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUShaderModule compute_shader, composite_shader;
    std::array<WGPUComputePipeline, 3> compute{};
    std::array<WGPUBindGroupLayout, 3> layouts{};
    WGPURenderPipeline composite;
    WGPUBindGroupLayout composite_layout;

    Fixture(WGPUInstance instance, WGPUDevice device) : instance(instance), device(device) {
        queue = wgpuDeviceGetQueue(device);
        const auto shader = [&](const char* code) {
            WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
            source.code = {code, WGPU_STRLEN};
            WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
            desc.nextInChain = &source.chain;
            return wgpuDeviceCreateShaderModule(device, &desc);
        };
        compute_shader = shader(midnafx::render::ambient_occlusion_compute_shader);
        composite_shader = shader(midnafx::render::ambient_occlusion_shader);
        const char* names[]{"cs_occlusion", "cs_filter_horizontal", "cs_filter_vertical"};
        for (unsigned i = 0; i < 3; ++i) {
            WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
            desc.compute.module = compute_shader; desc.compute.entryPoint = {names[i], WGPU_STRLEN};
            compute[i] = wgpuDeviceCreateComputePipeline(device, &desc);
            layouts[i] = wgpuComputePipelineGetBindGroupLayout(compute[i], 0);
        }
        WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
        color.format = WGPUTextureFormat_RGBA8Unorm; color.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = composite_shader; fragment.entryPoint = {"fs_ambient", WGPU_STRLEN};
        fragment.targetCount = 1; fragment.targets = &color;
        WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        desc.vertex.module = composite_shader; desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
        desc.fragment = &fragment;
        composite = wgpuDeviceCreateRenderPipeline(device, &desc);
        composite_layout = wgpuRenderPipelineGetBindGroupLayout(composite, 0);
    }
    ~Fixture() {
        wgpuBindGroupLayoutRelease(composite_layout);
        wgpuRenderPipelineRelease(composite);
        for (auto layout : layouts) wgpuBindGroupLayoutRelease(layout);
        for (auto pipeline : compute) wgpuComputePipelineRelease(pipeline);
        wgpuShaderModuleRelease(composite_shader); wgpuShaderModuleRelease(compute_shader);
        wgpuQueueRelease(queue);
    }

    Output run(unsigned width, unsigned height, const std::vector<float>& depths,
               const Params& params, const std::vector<Field>& seed = {}) {
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        const unsigned half_width = (width + 1) / 2, half_height = (height + 1) / 2;
        const unsigned color_stride = (width * 4 + 255) & ~255u;
        const unsigned field_stride = (half_width * 8 + 255) & ~255u;
        const size_t raw_offset = size_t(color_stride) * height;
        const size_t filtered_offset = raw_offset + size_t(field_stride) * half_height;
        WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
        td.size = {width, height, 1}; td.format = WGPUTextureFormat_RGBA8Unorm;
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        auto source = wgpuDeviceCreateTexture(device, &td);
        auto source_view = wgpuTextureCreateView(source, nullptr);
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        auto target = wgpuDeviceCreateTexture(device, &td);
        auto target_view = wgpuTextureCreateView(target, nullptr);
        td.format = WGPUTextureFormat_R32Float;
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        auto depth = wgpuDeviceCreateTexture(device, &td);
        auto depth_view = wgpuTextureCreateView(depth, nullptr);
        td.size = {half_width, half_height, 1}; td.format = WGPUTextureFormat_RGBA16Float;
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding |
                   WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
        std::array<WGPUTexture, 2> fields{};
        std::array<WGPUTextureView, 2> views{};
        for (unsigned i = 0; i < 2; ++i) {
            fields[i] = wgpuDeviceCreateTexture(device, &td);
            views[i] = wgpuTextureCreateView(fields[i], nullptr);
        }
        const auto upload = [&](WGPUTexture texture, const void* data, size_t size,
                                unsigned w, unsigned h, unsigned bytes_per_pixel) {
            WGPUTexelCopyTextureInfo dest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            dest.texture = texture;
            WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
            layout.bytesPerRow = w * bytes_per_pixel; layout.rowsPerImage = h;
            const WGPUExtent3D extent{w, h, 1};
            wgpuQueueWriteTexture(queue, &dest, data, size, &layout, &extent);
        };
        const auto pixels = scene(width, height);
        upload(source, pixels.data(), pixels.size() * sizeof(Pixel), width, height, 4);
        upload(depth, depths.data(), depths.size() * sizeof(float), width, height, 4);
        if (!seed.empty()) {
            std::vector<std::uint16_t> encoded(seed.size() * 4);
            for (size_t i = 0; i < seed.size(); ++i)
                for (unsigned c = 0; c < 4; ++c) {
                    // Validity plus attenuation occupies [1,1.5], beyond the
                    // LUT encoder's [0,1] contract; this half exponent is fixed.
                    encoded[i * 4 + c] = c == 1 && seed[i][c] >= 1 ?
                        static_cast<std::uint16_t>(0x3c00 + std::lround((seed[i][c] - 1) * 1024)) :
                        midnafx::lut::half(seed[i][c]);
                }
            upload(fields[0], encoded.data(), encoded.size() * 2, half_width, half_height, 8);
        }
        WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
        bd.size = sizeof(Params); bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        auto uniform = wgpuDeviceCreateBuffer(device, &bd);
        wgpuQueueWriteBuffer(queue, uniform, 0, &params, sizeof(params));
        bd.size = filtered_offset + size_t(field_stride) * half_height;
        bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        auto readback = wgpuDeviceCreateBuffer(device, &bd);
        std::array<WGPUBindGroup, 3> groups{};
        for (unsigned i = 0; i < 3; ++i) {
            WGPUBindGroupEntry entries[4];
            for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
            unsigned count = 0;
            entries[count].binding = 0; entries[count++].textureView = depth_view;
            if (i) { entries[count].binding = 1; entries[count++].textureView = views[i == 1 ? 0 : 1]; }
            entries[count].binding = 2; entries[count++].textureView = views[i == 1 ? 1 : 0];
            entries[count].binding = 3; entries[count].buffer = uniform; entries[count++].size = sizeof(params);
            WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
            desc.layout = layouts[i]; desc.entryCount = count; desc.entries = entries;
            groups[i] = wgpuDeviceCreateBindGroup(device, &desc);
        }
        WGPUBindGroupEntry entries[4];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entries[0].binding = 0; entries[0].textureView = source_view;
        entries[1].binding = 1; entries[1].textureView = depth_view;
        entries[2].binding = 2; entries[2].buffer = uniform; entries[2].size = sizeof(params);
        entries[3].binding = 3; entries[3].textureView = views[0];
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = composite_layout; desc.entryCount = 4; desc.entries = entries;
        auto group = wgpuDeviceCreateBindGroup(device, &desc);
        auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
        const auto dispatch = [&](unsigned index) {
            auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
            wgpuComputePassEncoderSetPipeline(pass, compute[index]);
            wgpuComputePassEncoderSetBindGroup(pass, 0, groups[index], 0, nullptr);
            wgpuComputePassEncoderDispatchWorkgroups(pass, (half_width + 7) / 8, (half_height + 7) / 8, 1);
            wgpuComputePassEncoderEnd(pass); wgpuComputePassEncoderRelease(pass);
        };
        const auto copy = [&](WGPUTexture texture, size_t offset, unsigned stride, unsigned w, unsigned h) {
            WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            source.texture = texture;
            WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
            destination.buffer = readback; destination.layout.offset = offset;
            destination.layout.bytesPerRow = stride; destination.layout.rowsPerImage = h;
            const WGPUExtent3D extent{w, h, 1};
            wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
        };
        if (seed.empty()) dispatch(0);
        copy(fields[0], raw_offset, field_stride, half_width, half_height);
        dispatch(1); dispatch(2);
        copy(fields[0], filtered_offset, field_stride, half_width, half_height);
        WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        attachment.view = target_view; attachment.loadOp = WGPULoadOp_Clear; attachment.storeOp = WGPUStoreOp_Store;
        WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        pass_desc.colorAttachmentCount = 1; pass_desc.colorAttachments = &attachment;
        auto pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
        wgpuRenderPassEncoderSetPipeline(pass, composite);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);
        copy(target, 0, color_stride, width, height);
        auto command = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(queue, 1, &command);
        bool mapped = false;
        WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_WaitAnyOnly; callback.userdata1 = &mapped;
        callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
            *static_cast<bool*>(user) = status == WGPUMapAsyncStatus_Success;
        };
        wait(instance, wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, bd.size, callback));
        Output output;
        if (mapped) {
            const auto* data = static_cast<const unsigned char*>(wgpuBufferGetConstMappedRange(readback, 0, bd.size));
            if (data) {
                output.color.resize(pixels.size());
                for (unsigned y = 0; y < height; ++y)
                    std::memcpy(output.color.data() + y * width, data + y * color_stride, width * 4);
                for (auto [offset, result] : {std::pair{raw_offset, &output.raw}, {filtered_offset, &output.filtered}}) {
                    result->resize(half_width * half_height);
                    for (unsigned y = 0; y < half_height; ++y)
                        for (unsigned x = 0; x < half_width; ++x) {
                            std::array<std::uint16_t, 4> value{};
                            std::memcpy(value.data(), data + offset + y * field_stride + x * 8, 8);
                            for (unsigned c = 0; c < 4; ++c) (*result)[y * half_width + x][c] = decode_half(value[c]);
                        }
                }
            }
            wgpuBufferUnmap(readback);
        }
        bool valid = false;
        WGPUPopErrorScopeCallbackInfo scope = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        scope.mode = WGPUCallbackMode_WaitAnyOnly; scope.userdata1 = &valid;
        scope.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type,
                            WGPUStringView message, void* user, void*) {
            *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success && type == WGPUErrorType_NoError;
            if (!*static_cast<bool*>(user)) std::fprintf(stderr, "Ambient validation: %.*s\n", int(message.length), message.data);
        };
        wait(instance, wgpuDevicePopErrorScope(device, scope));
        if (!valid) output = {};
        wgpuCommandBufferRelease(command); wgpuRenderPassEncoderRelease(pass); wgpuCommandEncoderRelease(encoder);
        wgpuBindGroupRelease(group); for (auto item : groups) wgpuBindGroupRelease(item);
        wgpuBufferRelease(readback); wgpuBufferRelease(uniform);
        for (auto view : views) wgpuTextureViewRelease(view);
        for (auto field : fields) wgpuTextureRelease(field);
        wgpuTextureViewRelease(depth_view); wgpuTextureRelease(depth);
        wgpuTextureViewRelease(target_view); wgpuTextureRelease(target);
        wgpuTextureViewRelease(source_view); wgpuTextureRelease(source);
        return output;
    }
};

bool bounded(const Output& output, unsigned width, unsigned height) {
    const auto source = scene(width, height);
    if (output.color.size() != source.size() || output.raw.size() != ((width + 1) / 2) * ((height + 1) / 2) ||
        output.filtered.size() != output.raw.size()) return false;
    for (size_t i = 0; i < source.size(); ++i) if (output.color[i][3] != source[i][3]) return false;
    for (const auto* field : {&output.raw, &output.filtered})
        for (const auto& value : *field) {
            for (float channel : value) if (!std::isfinite(channel)) return false;
            if (value[0] < 0 || value[0] > 1 ||
                (value[1] != 0 && (value[1] < 1 || value[1] > 1.5f))) return false;
        }
    return true;
}
float variation(const std::vector<Field>& field, unsigned width, unsigned height, unsigned channel = 0) {
    float total = 0;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            if (x) total += std::abs(field[y * width + x][channel] - field[y * width + x - 1][channel]);
            if (y) total += std::abs(field[y * width + x][channel] - field[(y - 1) * width + x][channel]);
        }
    return total;
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
    WGPURequestAdapterCallbackInfo adapter_callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapter_callback.mode = WGPUCallbackMode_WaitAnyOnly; adapter_callback.userdata1 = &adapter;
    adapter_callback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter value, WGPUStringView, void* user, void*) {
        if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(user) = value;
    };
    wait(instance, wgpuInstanceRequestAdapter(instance, &options, adapter_callback));
    if (!adapter) { wgpuInstanceRelease(instance); return 77; }
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
    wgpuAdapterGetInfo(adapter, &info);
    std::printf("Adapter: %.*s; backend=%u\n", int(info.device.length), info.device.data, unsigned(info.backendType));
    wgpuAdapterInfoFreeMembers(info);
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo device_callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    device_callback.mode = WGPUCallbackMode_WaitAnyOnly; device_callback.userdata1 = &device;
    device_callback.callback = [](WGPURequestDeviceStatus status, WGPUDevice value, WGPUStringView message, void* user, void*) {
        if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(user) = value;
        else std::fprintf(stderr, "Device request: %.*s\n", int(message.length), message.data);
    };
    wait(instance, wgpuAdapterRequestDevice(adapter, nullptr, device_callback));
    if (!device) { wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance); return 1; }
    bool ok = true;
    unsigned cases = 0;
    const auto report = [&](const char* label, bool passed) {
        std::printf("%s: %s\n", label, passed ? "PASS" : "FAIL"); ok &= passed; ++cases;
    };
    {
        Fixture fixture(instance, device);
        for (auto size : {std::array<unsigned, 2>{96, 80}, {65, 49}, {1, 1}}) {
            const auto [width, height] = size;
            const auto output = fixture.run(width, height, std::vector<float>(width * height, depth_at(10)), perspective(width, height));
            char label[80]; std::snprintf(label, sizeof(label), "Flat perspective plane %ux%u identity", width, height);
            report(label, bounded(output, width, height) && output.color == scene(width, height));
        }
        {
            const unsigned width = 33, height = 25;
            const auto output = fixture.run(width, height, std::vector<float>(width * height, 1), perspective(width, height));
            bool valid = bounded(output, width, height) && output.color == scene(width, height);
            for (const auto& value : output.filtered) valid &= value[0] == 1 && value[1] == 0;
            report("All background identity and invalid field", valid);
        }
        for (auto size : {std::array<unsigned, 2>{96, 80}, {65, 49}}) {
            const auto [width, height] = size;
            std::vector<float> depth(width * height, depth_at(10));
            for (unsigned y = height / 3; y < 2 * height / 3; ++y)
                for (unsigned x = width / 3; x < 2 * width / 3; ++x) depth[y * width + x] = depth_at(9);
            const auto output = fixture.run(width, height, depth, perspective(width, height));
            bool valid = bounded(output, width, height);
            unsigned darkened = 0, max_drop = 0;
            if (valid) for (const auto& pixel : output.color) {
                valid &= pixel[0] <= 120 && pixel[1] <= 160 && pixel[2] <= 200;
                darkened += pixel[0] < 120; max_drop = std::max(max_drop, 120u - pixel[0]);
            }
            valid &= darkened > 0 && max_drop >= 2;
            char label[100]; std::snprintf(label, sizeof(label), "Raised box contacts %ux%u darkened=%u maxdrop=%u", width, height, darkened, max_drop);
            report(label, valid);
        }
        {
            const unsigned width = 64, height = 48, half_width = width / 2, half_height = height / 2;
            std::vector<Field> seed(half_width * half_height);
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x) seed[y * half_width + x] = {(x + y) % 2 ? 0.55f : 1.0f, 1, 0.5f, 0.5f};
            const auto output = fixture.run(width, height, std::vector<float>(width * height, depth_at(10)), perspective(width, height), seed);
            bool valid = bounded(output, width, height);
            if (valid) {
                valid &= variation(output.filtered, half_width, half_height) < variation(output.raw, half_width, half_height) * 0.25f;
                float mean = 0; for (const auto& value : output.filtered) mean += value[0];
                valid &= std::abs(mean / output.filtered.size() - 0.775f) < 0.01f;
            }
            report("Separable filter reduces same-plane visibility variation", valid);
            std::vector<float> depth(width * height, depth_at(10));
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = width / 2; x < width; ++x) depth[y * width + x] = depth_at(40);
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x) seed[y * half_width + x] = {x < half_width / 2 ? 0.55f : 1.0f, 1, 0.5f, 0.5f};
            const auto isolated = fixture.run(width, height, depth, perspective(width, height), seed);
            valid = bounded(isolated, width, height);
            const auto source = scene(width, height);
            if (valid) {
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = width / 2; x < width; ++x) valid &= isolated.color[y * width + x] == source[y * width + x];
                for (unsigned y = 0; y < half_height; ++y)
                    for (unsigned x = 0; x < half_width; ++x)
                        valid &= std::abs(isolated.filtered[y * half_width + x][0] - (x < half_width / 2 ? 0.55f : 1.0f)) < 0.001f;
                valid &= isolated.color[width + width / 4][0] < 110;
            }
            report("Distant foreground/background planes reject filter and upsample bleed", valid);
        }
        {
            const unsigned width = 33, height = 25;
            auto params = perspective(width, height); params.strength = 0;
            params.fog_color[0] = 0.2f; params.fog_color[1] = 0.3f; params.fog_color[2] = 0.4f; params.fog_color[3] = 0.35f;
            params.fog_range[0] = 2; params.fog_range[1] = 18;
            const auto output = fixture.run(width, height, std::vector<float>(width * height, depth_at(10)), params);
            const float amount = 0.35f * (1 - std::exp(-1.0f));
            const auto source = scene(width, height);
            bool valid = bounded(output, width, height) && output.color != source;
            if (valid) for (size_t i = 0; i < source.size(); ++i)
                for (unsigned c = 0; c < 3; ++c) {
                    const int expected = int(std::lround(float(source[i][c]) * (1 - amount) + params.fog_color[c] * 255 * amount));
                    valid &= std::abs(int(output.color[i][c]) - expected) <= 1;
                }
            params.strength = 1;
            const auto fallback = fixture.run(width, height, std::vector<float>(width * height, depth_at(10)), params,
                std::vector<Field>(((width + 1) / 2) * ((height + 1) / 2), Field{0, 0, 0, 0}));
            valid &= bounded(fallback, width, height) && fallback.color == output.color;
            report("Zero AO strength retains independent fog formula and alpha", valid);
        }
        {
            const unsigned width = 64, height = 48, half_width = width / 2, half_height = height / 2;
            auto params = perspective(width, height); params.strength = 0;
            params.light_position[3] = 0.4f;
            std::vector<Field> seed(half_width * half_height);
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x)
                    seed[y * half_width + x] = {1, (x + y) % 2 ? 1.4f : 1.0f, 0.5f, 0.5f};
            std::vector<float> depth(width * height, depth_at(10));
            const auto smooth = fixture.run(width, height, depth, params, seed);
            bool valid = bounded(smooth, width, height);
            if (valid) {
                valid &= variation(smooth.filtered, half_width, half_height, 1) <
                         variation(smooth.raw, half_width, half_height, 1) * 0.25f;
                float mean = 0; for (const auto& value : smooth.filtered) mean += value[1];
                valid &= std::abs(mean / smooth.filtered.size() - 1.2f) < 0.005f;
            }
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = width / 2; x < width; ++x) depth[y * width + x] = depth_at(40);
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x)
                    seed[y * half_width + x] = {1, x < half_width / 2 ? 1.4f : 1.0f, 0.5f, 0.5f};
            const auto isolated = fixture.run(width, height, depth, params, seed);
            valid &= bounded(isolated, width, height);
            if (isolated.filtered.size() == seed.size())
                for (size_t i = 0; i < seed.size(); ++i)
                    valid &= std::abs(isolated.filtered[i][1] - seed[i][1]) < 0.001f;
            const auto source = scene(width, height);
            if (isolated.color.size() == source.size())
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = width / 2; x < width; ++x)
                        valid &= isolated.color[y * width + x] == source[y * width + x];
            report("Shadow attenuation filters smoothly within a plane and rejects depth-step bleed", valid);
            params.strength = 1; params.light_position[3] = 0.5f;
            std::fill(depth.begin(), depth.end(), depth_at(10));
            std::fill(seed.begin(), seed.end(), Field{0, 1.5f, 0.5f, 0.5f});
            const auto capped = fixture.run(width, height, depth, params, seed);
            valid = bounded(capped, width, height);
            for (const auto& pixel : capped.color)
                valid &= pixel[0] == 60 && pixel[1] == 80 && pixel[2] == 100;
            report("Combined AO and shadow darkness stays capped at fifty percent", valid);
        }
        for (auto size : {std::array<unsigned, 2>{96, 80}, {65, 49}}) {
            const auto [width, height] = size;
            auto params = perspective(width, height); params.strength = 0;
            params.light_position[0] = 8; params.light_position[2] = -4; params.light_position[3] = 0.4f;
            params.shadow_range[0] = 4;
            const std::vector<float> plane(width * height, depth_at(10));
            auto depth = plane;
            for (unsigned y = height / 3; y < 2 * height / 3; ++y)
                for (unsigned x = width / 2 - width / 12; x < width / 2 + width / 12; ++x)
                    depth[y * width + x] = depth_at(8.5f);
            const auto from_right = fixture.run(width, height, depth, params);
            params.light_position[0] = -8;
            const auto from_left = fixture.run(width, height, depth, params);
            struct Stats { unsigned darkened = 0, max_drop = 0; double weight = 0, weighted_x = 0; };
            const auto measure = [&](const Output& output) {
                Stats stats;
                if (output.color.size() != depth.size()) return stats;
                for (unsigned y = 2; y + 2 < height; ++y)
                    for (unsigned x = 2; x + 2 < width; ++x) {
                        const size_t i = y * width + x;
                        if (depth[i] != depth_at(10)) continue;
                        const unsigned drop = unsigned(std::max(120 - int(output.color[i][0]), 0));
                        stats.darkened += drop > 0; stats.max_drop = std::max(stats.max_drop, drop);
                        stats.weight += drop; stats.weighted_x += drop * x;
                    }
                return stats;
            };
            const auto right = measure(from_right), left = measure(from_left);
            const double right_centroid = right.weight ? right.weighted_x / right.weight : -1;
            const double left_centroid = left.weight ? left.weighted_x / left.weight : -1;
            bool valid = bounded(from_right, width, height) && bounded(from_left, width, height) &&
                         right.darkened > 5 && left.darkened > 5 && right.max_drop >= 2 && left.max_drop >= 2 &&
                         right_centroid < width * 0.5 && left_centroid > width * 0.5 &&
                         left_centroid - right_centroid > 2;
            for (const auto* output : {&from_right, &from_left})
                for (const auto& pixel : output->color)
                    valid &= pixel[0] >= 60 && pixel[0] <= 120 && pixel[1] >= 80 && pixel[1] <= 160 &&
                             pixel[2] >= 100 && pixel[2] <= 200;
            char label[180];
            std::snprintf(label, sizeof(label),
                "Directional cast %ux%u receiver counts=%u/%u maxdrop=%u/%u centroids=%.2f/%.2f",
                width, height, right.darkened, left.darkened, right.max_drop, left.max_drop, right_centroid, left_centroid);
            report(label, valid);

            const auto no_caster = fixture.run(width, height, plane, params);
            params.light_position[2] = -14;
            const auto behind = fixture.run(width, height, depth, params);
            const auto source = scene(width, height);
            valid = bounded(no_caster, width, height) && bounded(behind, width, height) &&
                    no_caster.color == source && behind.color == source;
            std::snprintf(label, sizeof(label), "No-caster plane and light-behind-surface identity %ux%u", width, height);
            report(label, valid);

            if (width == 96) {
                auto legacy = perspective(width, height);
                const auto baseline = fixture.run(width, height, depth, legacy);
                legacy.light_position[0] = 8; legacy.light_position[2] = -4;
                legacy.shadow_range[0] = 4;
                const auto disabled = fixture.run(width, height, depth, legacy);
                report("Zero shadow strength preserves old AO color and field exactly",
                    bounded(baseline, width, height) && bounded(disabled, width, height) &&
                    disabled.color == baseline.color && disabled.raw == baseline.raw && disabled.filtered == baseline.filtered);

                auto reversed_params = perspective(width, height); reversed_params.strength = 0;
                reversed_params.light_position[0] = 8; reversed_params.light_position[2] = -4;
                reversed_params.light_position[3] = 0.4f; reversed_params.shadow_range[0] = 4;
                reversed_params.background_depth = 0;
                reversed_params.proj_from_view[10] = 1.0f / 99;
                reversed_params.proj_from_view[14] = 100.0f / 99;
                reversed_params.view_from_proj[11] = 99.0f / 100;
                reversed_params.view_from_proj[15] = 1.0f / 100;
                auto reversed_depth = depth;
                for (auto& value : reversed_depth) value = 1 - value;
                const auto reversed = fixture.run(width, height, reversed_depth, reversed_params);
                valid = bounded(reversed, width, height) && bounded(from_right, width, height);
                if (valid) {
                    // Equivalent projection inversions may round at adjacent
                    // half-float/UNorm codes; geometry and attenuation must agree.
                    for (size_t i = 0; i < reversed.color.size(); ++i) {
                        valid &= reversed.color[i][3] == from_right.color[i][3];
                        for (unsigned c = 0; c < 3; ++c)
                            valid &= std::abs(int(reversed.color[i][c]) - int(from_right.color[i][c])) <= 1;
                    }
                    for (size_t i = 0; i < reversed.raw.size(); ++i)
                        for (unsigned c = 0; c < 2; ++c)
                            valid &= std::abs(reversed.raw[i][c] - from_right.raw[i][c]) <= 0.002f;
                }
                report("Reversed-Z matched projection preserves directional shadow and alpha", valid);

                auto fog = perspective(width, height); fog.strength = 0;
                fog.light_position[0] = 8; fog.light_position[2] = -4; fog.light_position[3] = 0.4f;
                fog.shadow_range[0] = 4;
                fog.fog_color[0] = 0.2f; fog.fog_color[1] = 0.3f; fog.fog_color[2] = 0.4f; fog.fog_color[3] = 0.3f;
                fog.fog_range[0] = 2; fog.fog_range[1] = 18;
                const auto hazy = fixture.run(width, height, depth, fog);
                valid = bounded(hazy, width, height) && from_right.color.size() == depth.size();
                if (valid) for (size_t i = 0; i < depth.size(); ++i) {
                    const float distance = depth[i] == depth_at(10) ? 10.0f : 8.5f;
                    const float amount = 0.3f * (1 - std::exp(-2 * (distance - 2) / 16));
                    for (unsigned c = 0; c < 3; ++c) {
                        const int expected = int(std::lround(from_right.color[i][c] * (1 - amount) + fog.fog_color[c] * 255 * amount));
                        valid &= std::abs(int(hazy.color[i][c]) - expected) <= 1;
                    }
                }
                report("Directional shadow composes before independent fog and retains alpha", valid);
            }
        }
        {
            constexpr unsigned width = 192, height = 192, caster_x = 101, caster_y = 95;
            auto params = perspective(width, height); params.strength = 0;
            // The visible caster is 0.1 world units beyond the close point
            // light, with exactly the same view-space X/Y at its pixel center.
            // Score only receiver pixels; the caster's own shading is unrelated.
            params.light_position[0] = (2 * (caster_x + 0.5f) / width - 1) * 8.9f;
            params.light_position[1] = (1 - 2 * (caster_y + 0.5f) / height) * 8.9f;
            params.light_position[2] = -9; params.light_position[3] = 0.4f;
            params.shadow_range[0] = 4;
            std::vector<float> depth(width * height, depth_at(10));
            depth[caster_y * width + caster_x] = depth_at(8.9f);
            const auto output = fixture.run(width, height, depth, params);
            const auto source = scene(width, height);
            bool valid = bounded(output, width, height);
            if (valid) {
                for (unsigned y = 93; y <= 97; y += 2)
                    for (unsigned x = 91; x <= 97; x += 2) {
                        const auto& field = output.raw[((y - 1) / 2) * (width / 2) + (x - 1) / 2];
                        valid &= field[0] == 1 && field[1] == 1;
                    }
                for (unsigned y = 91; y <= 99; ++y)
                    for (unsigned x = 91; x <= 95; ++x)
                        valid &= output.color[y * width + x] == source[y * width + x];
            }
            report("Close point light rejects a visible caster beyond its endpoint on the receiver", valid);
        }
        {
            bool valid = true;
            for (auto size : {std::array<unsigned, 2>{1, 1}, {3, 5}, {33, 25}}) {
                const auto [width, height] = size;
                auto params = perspective(width, height); params.strength = 0;
                params.light_position[0] = 8; params.light_position[2] = -4; params.light_position[3] = 0.4f;
                params.shadow_range[0] = 4;
                const auto output = fixture.run(width, height, std::vector<float>(width * height, depth_at(10)), params);
                valid &= bounded(output, width, height) && output.color == scene(width, height);
            }
            report("Small and odd shadow targets remain finite and preserve a flat plane", valid);
        }
    }
    std::printf("%u ambient pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
    wgpuDeviceRelease(device); wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
