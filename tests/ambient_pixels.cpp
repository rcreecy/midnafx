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
    float bounce[4]{};
    float sun_screen[4]{};
    float sun_color[4]{};
};
static_assert(sizeof(Params) == 256);

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
    std::vector<Field> raw_bounce, filtered_bounce;
};
struct Fixture {
    WGPUInstance instance;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUShaderModule compute_shader, composite_shader;
    std::array<WGPUComputePipeline, 4> compute{};
    std::array<WGPUBindGroupLayout, 4> layouts{};
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
        const char* names[]{"cs_occlusion", "cs_bounce", "cs_filter_horizontal", "cs_filter_vertical"};
        for (unsigned i = 0; i < 4; ++i) {
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
               const Params& params, const std::vector<Field>& seed = {},
               const std::vector<Pixel>& source_pixels = {},
               const std::vector<Field>& bounce_seed = {}) {
        if (depths.size() != size_t(width) * height ||
            (!source_pixels.empty() && source_pixels.size() != depths.size())) return {};
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        const unsigned half_width = (width + 1) / 2, half_height = (height + 1) / 2;
        const unsigned bounce_width = params.bounce[0] > 0 ? half_width : 1;
        const unsigned bounce_height = params.bounce[0] > 0 ? half_height : 1;
        const unsigned color_stride = (width * 4 + 255) & ~255u;
        const unsigned field_stride = (half_width * 8 + 255) & ~255u;
        const unsigned bounce_stride = (bounce_width * 8 + 255) & ~255u;
        const size_t raw_offset = size_t(color_stride) * height;
        const size_t filtered_offset = raw_offset + size_t(field_stride) * half_height;
        const size_t raw_bounce_offset = filtered_offset + size_t(field_stride) * half_height;
        const size_t filtered_bounce_offset = raw_bounce_offset + size_t(bounce_stride) * bounce_height;
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
        td.size = {bounce_width, bounce_height, 1};
        std::array<WGPUTexture, 2> bounce_fields{};
        std::array<WGPUTextureView, 2> bounce_views{};
        for (unsigned i = 0; i < 2; ++i) {
            bounce_fields[i] = wgpuDeviceCreateTexture(device, &td);
            bounce_views[i] = wgpuTextureCreateView(bounce_fields[i], nullptr);
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
        const auto pixels = source_pixels.empty() ? scene(width, height) : source_pixels;
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
        if (!bounce_seed.empty()) {
            std::vector<std::uint16_t> encoded(bounce_seed.size() * 4);
            for (size_t i = 0; i < bounce_seed.size(); ++i)
                for (unsigned c = 0; c < 4; ++c)
                    encoded[i * 4 + c] = midnafx::lut::half(bounce_seed[i][c]);
            upload(bounce_fields[0], encoded.data(), encoded.size() * 2, bounce_width, bounce_height, 8);
        }
        WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
        bd.size = sizeof(Params); bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        auto uniform = wgpuDeviceCreateBuffer(device, &bd);
        wgpuQueueWriteBuffer(queue, uniform, 0, &params, sizeof(params));
        bd.size = filtered_bounce_offset + size_t(bounce_stride) * bounce_height;
        bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        auto readback = wgpuDeviceCreateBuffer(device, &bd);
        std::array<WGPUBindGroup, 4> groups{};
        for (unsigned i = 0; i < 4; ++i) {
            WGPUBindGroupEntry entries[7];
            for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
            unsigned count = 0;
            entries[count].binding = 0; entries[count++].textureView = depth_view;
            if (i) { entries[count].binding = 1; entries[count++].textureView = views[i == 3 ? 1 : 0]; }
            if (i != 1) {
                entries[count].binding = 2; entries[count++].textureView = views[i == 2 ? 1 : 0];
            }
            entries[count].binding = 3; entries[count].buffer = uniform; entries[count++].size = sizeof(params);
            if (i == 1) { entries[count].binding = 4; entries[count++].textureView = source_view; }
            if (i >= 2) {
                entries[count].binding = 5; entries[count++].textureView = bounce_views[i == 2 ? 0 : 1];
            }
            if (i) { entries[count].binding = 6; entries[count++].textureView = bounce_views[i == 2 ? 1 : 0]; }
            WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
            desc.layout = layouts[i]; desc.entryCount = count; desc.entries = entries;
            groups[i] = wgpuDeviceCreateBindGroup(device, &desc);
        }
        WGPUBindGroupEntry entries[5];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entries[0].binding = 0; entries[0].textureView = source_view;
        entries[1].binding = 1; entries[1].textureView = depth_view;
        entries[2].binding = 2; entries[2].buffer = uniform; entries[2].size = sizeof(params);
        entries[3].binding = 3; entries[3].textureView = views[0];
        entries[4].binding = 4; entries[4].textureView = bounce_views[0];
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = composite_layout; desc.entryCount = 5; desc.entries = entries;
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
        if (bounce_seed.empty()) dispatch(1);
        copy(bounce_fields[0], raw_bounce_offset, bounce_stride, bounce_width, bounce_height);
        dispatch(2); dispatch(3);
        copy(fields[0], filtered_offset, field_stride, half_width, half_height);
        copy(bounce_fields[0], filtered_bounce_offset, bounce_stride, bounce_width, bounce_height);
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
                struct FieldRead {
                    size_t offset;
                    std::vector<Field>* result;
                    unsigned width, height, stride;
                };
                for (const auto& read : {
                         FieldRead{raw_offset, &output.raw, half_width, half_height, field_stride},
                         FieldRead{filtered_offset, &output.filtered, half_width, half_height, field_stride},
                         FieldRead{raw_bounce_offset, &output.raw_bounce, bounce_width, bounce_height, bounce_stride},
                         FieldRead{filtered_bounce_offset, &output.filtered_bounce, bounce_width, bounce_height, bounce_stride}}) {
                    read.result->resize(read.width * read.height);
                    for (unsigned y = 0; y < read.height; ++y)
                        for (unsigned x = 0; x < read.width; ++x) {
                            std::array<std::uint16_t, 4> value{};
                            std::memcpy(value.data(), data + read.offset + y * read.stride + x * 8, 8);
                            for (unsigned c = 0; c < 4; ++c) (*read.result)[y * read.width + x][c] = decode_half(value[c]);
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
        for (auto view : bounce_views) wgpuTextureViewRelease(view);
        for (auto field : bounce_fields) wgpuTextureRelease(field);
        wgpuTextureViewRelease(depth_view); wgpuTextureRelease(depth);
        wgpuTextureViewRelease(target_view); wgpuTextureRelease(target);
        wgpuTextureViewRelease(source_view); wgpuTextureRelease(source);
        return output;
    }
};

bool bounded(const Output& output, unsigned width, unsigned height,
             const std::vector<Pixel>& source_pixels = {}) {
    const auto source = source_pixels.empty() ? scene(width, height) : source_pixels;
    if (output.color.size() != source.size() || output.raw.size() != ((width + 1) / 2) * ((height + 1) / 2) ||
        output.filtered.size() != output.raw.size()) return false;
    for (size_t i = 0; i < source.size(); ++i) if (output.color[i][3] != source[i][3]) return false;
    for (const auto* field : {&output.raw, &output.filtered})
        for (const auto& value : *field) {
            for (float channel : value) if (!std::isfinite(channel)) return false;
            if (value[0] < 0 || value[0] > 1 ||
                (value[1] != 0 && (value[1] < 1 || value[1] > 1.5f))) return false;
        }
    if (output.raw_bounce.empty() || output.raw_bounce.size() != output.filtered_bounce.size()) return false;
    for (const auto* field : {&output.raw_bounce, &output.filtered_bounce})
        for (const auto& value : *field)
            for (float channel : value) if (!std::isfinite(channel) || channel < 0 || channel > 1) return false;
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

// Two camera-visible walls meet in a concave vertical corner. Their inward
// normals face one another, unlike the parallel raised-box AO fixture.
std::vector<float> corner_depth(unsigned width, unsigned height) {
    std::vector<float> result(width * height);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            const float ray_x = (2 * (x + 0.5f) / width - 1) * float(width) / height;
            result[y * width + x] = depth_at(10 / (1 + std::abs(ray_x)));
        }
    return result;
}
std::vector<Pixel> corner_colors(unsigned width, unsigned height, Pixel receiver, Pixel donor) {
    auto result = scene(width, height);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            for (unsigned c = 0; c < 3; ++c)
                result[y * width + x][c] = (x < width / 2 ? receiver : donor)[c];
    return result;
}
bool bounce_bounds(const Output& output, unsigned width, unsigned height,
                   const std::vector<Pixel>& source, float strength) {
    if (!bounded(output, width, height, source)) return false;
    // Independent energy/headroom bound. Do not reproduce the spatial gather.
    const auto linear = [](float value) {
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };
    const auto display = [](float value) {
        return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1 / 2.4f) - 0.055f;
    };
    for (size_t i = 0; i < source.size(); ++i)
        for (unsigned c = 0; c < 3; ++c) {
            const float base = linear(source[i][c] / 255.0f);
            const int upper = int(std::ceil(255 * display(base + strength * std::min(base, 1 - base))));
            if (int(output.color[i][c]) < int(source[i][c]) - 1 || int(output.color[i][c]) > upper + 1)
                return false;
        }
    return true;
}
int sunlight_code(unsigned char base, float color, float amount) {
    const auto linear = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    const float value = linear(base / 255.0f);
    const float result = value + (1 - value) * linear(color) * amount;
    const float encoded = result <= 0.0031308f ? result * 12.92f : 1.055f * std::pow(result, 1 / 2.4f) - 0.055f;
    return int(std::lround(encoded * 255));
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
        for (auto size : {std::array<unsigned, 2>{96, 96}, {39, 27}}) {
            const auto [width, height] = size;
            for (bool transpose : {false, true}) {
                const unsigned extent = transpose ? height : width;
                const unsigned boundary = ((extent / 2 - 1) | 1u);
                const unsigned receiver_x = transpose ? (width / 2 | 1u) : boundary;
                const unsigned receiver_y = transpose ? boundary : (height / 2 | 1u);
                // A clipped steep plane ends at the receiver. Its reciprocal
                // distance is affine in screen position, as required by its
                // equation -6*X+Z=-offset (or 6*Y+Z=-offset when transposed).
                // The separate wall beyond that edge is behind this plane,
                // but its smaller view-Z delta makes the old derivative choose
                // the wrong surface and reverse the normal's steep component.
                const float center_ray = (2 * (boundary + 0.5f) - extent) / height;
                const float offset = 10 * (1 + 6 * center_ray);
                std::vector<float> depths(width * height, depth_at(11));
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const unsigned coordinate = transpose ? y : x;
                        if (coordinate > boundary) continue;
                        const float ray = (2 * (coordinate + 0.5f) - extent) / height;
                        const float distance = offset / (1 + 6 * ray);
                        depths[y * width + x] = distance > 1 && distance < 100 ? depth_at(distance) : 1;
                    }
                for (bool reversed_z : {false, true}) {
                    auto params = perspective(width, height);
                    params.radius = 20; // The steep plane's first-neighbor displacement remains valid.
                    auto input = depths;
                    if (reversed_z) {
                        params.background_depth = 0;
                        params.proj_from_view[10] = 1.0f / 99;
                        params.proj_from_view[14] = 100.0f / 99;
                        params.view_from_proj[11] = 99.0f / 100;
                        params.view_from_proj[15] = 1.0f / 100;
                        for (auto& depth : input) depth = 1 - depth;
                    }
                    const auto output = fixture.run(width, height, input, params);
                    bool normal_valid = bounded(output, width, height);
                    float normal_dot = -1, visibility = -1;
                    if (normal_valid) {
                        const auto field = output.raw[(receiver_y / 2) * ((width + 1) / 2) + receiver_x / 2];
                        // Decode the public octahedral field representation;
                        // the expected direction comes from the analytic plane.
                        float nx = field[2] * 2 - 1, ny = field[3] * 2 - 1;
                        const float nz = 1 - std::abs(nx) - std::abs(ny);
                        const float fold = std::max(-nz, 0.0f);
                        nx += nx >= 0 ? -fold : fold;
                        ny += ny >= 0 ? -fold : fold;
                        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
                        normal_dot = (transpose ? 6 * ny + nz : -6 * nx + nz) /
                                     (std::sqrt(37.0f) * length);
                        visibility = field[0];
                        normal_valid &= field[1] >= 1 && normal_dot > std::cos(2.0f * 3.14159265358979323846f / 180);
                    }
                    char label[180];
                    std::snprintf(label, sizeof(label),
                        "Steep plane edge normal %ux%u transpose=%u reversed=%u dot=%.6f",
                        width, height, unsigned(transpose), unsigned(reversed_z), normal_dot);
                    report(label, normal_valid);
                    bool no_false_contact = bounded(output, width, height) && visibility >= 0.999f;
                    if (no_false_contact) {
                        const auto source = scene(width, height)[receiver_y * width + receiver_x];
                        const auto result = output.color[receiver_y * width + receiver_x];
                        for (unsigned channel = 0; channel < 3; ++channel)
                            no_false_contact &= std::abs(int(result[channel]) - int(source[channel])) <= 1;
                        no_false_contact &= result[3] == source[3];
                    }
                    std::snprintf(label, sizeof(label),
                        "Steep plane edge rejects false AO %ux%u transpose=%u reversed=%u visibility=%.6f",
                        width, height, unsigned(transpose), unsigned(reversed_z), visibility);
                    report(label, no_false_contact);
                }
            }
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
        {
            constexpr unsigned width = 65, height = 49;
            const auto depth = corner_depth(width, height);
            const auto colors = corner_colors(width, height, Pixel{120,120,120,0}, Pixel{220,25,25,0});
            auto params = perspective(width, height);
            params.light_position[0] = 8; params.light_position[2] = -4;
            params.light_position[3] = 0.3f; params.shadow_range[0] = 4;
            const auto baseline = fixture.run(width, height, depth, params, {}, colors);
            params.bounce[1] = 4;
            // Poison the actual one-pixel disabled field. Neither filtering nor
            // composition may consume it when bounce strength is zero.
            const auto disabled = fixture.run(width, height, depth, params, {}, colors,
                                               std::vector<Field>{Field{1,1,1,1}});
            report("Zero bounce strength ignores one-pixel dummy and preserves AO/shadow exactly",
                   bounded(baseline, width, height, colors) && bounded(disabled, width, height, colors) &&
                   baseline.color == disabled.color && baseline.raw == disabled.raw &&
                   baseline.filtered == disabled.filtered && disabled.filtered_bounce.size() == 1);
        }
        for (auto size : {std::array<unsigned,2>{96,80}, {65,49}}) {
            const auto [width, height] = size;
            const auto depth = corner_depth(width, height);
            const auto red = corner_colors(width, height, Pixel{120,120,120,0}, Pixel{220,25,25,0});
            const auto blue = corner_colors(width, height, Pixel{120,120,120,0}, Pixel{25,25,220,0});
            auto params = perspective(width, height); params.strength = 0;
            params.bounce[0] = 0.12f; params.bounce[1] = 4;
            const auto from_red = fixture.run(width, height, depth, params, {}, red);
            const auto from_blue = fixture.run(width, height, depth, params, {}, blue);
            bool valid = bounce_bounds(from_red, width, height, red, params.bounce[0]) &&
                         bounce_bounds(from_blue, width, height, blue, params.bounce[0]);
            unsigned red_lifted = 0, blue_lifted = 0;
            if (valid) for (unsigned y = 8; y + 8 < height; ++y)
                for (unsigned x = 0; x + 1 < width / 2; ++x) {
                    const size_t i = size_t(y) * width + x;
                    red_lifted += from_red.color[i][0] > red[i][0] &&
                                  from_red.color[i][0] > from_blue.color[i][0];
                    blue_lifted += from_blue.color[i][2] > blue[i][2] &&
                                   from_blue.color[i][2] > from_red.color[i][2];
                }
            valid &= red_lifted > 0 && blue_lifted > 0;
            char label[144];
            std::snprintf(label, sizeof(label), "Default 12%% corner transfers donor hue within energy bound %ux%u red=%u blue=%u",
                          width, height, red_lifted, blue_lifted);
            report(label, valid);

            auto tiny_radius = params; tiny_radius.bounce[1] = 0.05f;
            const auto distant = fixture.run(width, height, depth, tiny_radius, {}, red);
            valid = bounded(distant, width, height, red) && distant.color == red;
            for (const auto& value : distant.raw_bounce)
                valid &= value[0] == 0 && value[1] == 0 && value[2] == 0;
            report("Sources outside the bounce radius contribute no energy", valid);

            if (width == 96) {
                auto reversed_params = params;
                reversed_params.background_depth = 0;
                reversed_params.proj_from_view[10] = 1.0f / 99;
                reversed_params.proj_from_view[14] = 100.0f / 99;
                reversed_params.view_from_proj[11] = 99.0f / 100;
                reversed_params.view_from_proj[15] = 1.0f / 100;
                auto reversed_depth = depth;
                for (auto& value : reversed_depth) value = 1 - value;
                const auto reversed = fixture.run(width, height, reversed_depth, reversed_params, {}, red);
                valid = bounded(reversed, width, height, red) && reversed.color.size() == from_red.color.size();
                if (valid) for (size_t i = 0; i < from_red.color.size(); ++i)
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= std::abs(int(reversed.color[i][c]) - int(from_red.color[i][c])) <= 1;
                report("Reversed-Z corner keeps color bounce and alpha within one display code", valid);

                auto fog = params;
                fog.fog_color[0] = 0.2f; fog.fog_color[1] = 0.3f;
                fog.fog_color[2] = 0.4f; fog.fog_color[3] = 0.3f;
                fog.fog_range[0] = 2; fog.fog_range[1] = 18;
                const auto hazy = fixture.run(width, height, depth, fog, {}, red);
                valid = bounded(hazy, width, height, red) && hazy.color.size() == from_red.color.size();
                if (valid) for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const size_t i = size_t(y) * width + x;
                        const float ray_x = (2 * (x + 0.5f) / width - 1) * float(width) / height;
                        const float distance = 10 / (1 + std::abs(ray_x));
                        const float amount = 0.3f * (1 - std::exp(-2 * (distance - 2) / 16));
                        for (unsigned c = 0; c < 3; ++c) {
                            const int expected = int(std::lround(from_red.color[i][c] * (1 - amount) +
                                                                fog.fog_color[c] * 255 * amount));
                            valid &= std::abs(int(hazy.color[i][c]) - expected) <= 1;
                        }
                    }
                report("Color bounce composes before native-colored fog with exact source alpha", valid);

                bool endpoints = true;
                for (unsigned level : {0u, 255u}) {
                    const auto colors = corner_colors(width, height,
                        Pixel{static_cast<unsigned char>(level), static_cast<unsigned char>(level),
                              static_cast<unsigned char>(level), 0}, Pixel{220,25,25,0});
                    const auto output = fixture.run(width, height, depth, params, {}, colors);
                    endpoints &= bounded(output, width, height, colors);
                    if (output.color.size() == colors.size())
                        for (unsigned y = 0; y < height; ++y)
                            for (unsigned x = 0; x < width / 2; ++x)
                                endpoints &= output.color[y * width + x] == colors[y * width + x];
                }
                const auto black_donor = corner_colors(width, height, Pixel{120,120,120,0}, Pixel{0,0,0,0});
                const auto black = fixture.run(width, height, depth, params, {}, black_donor);
                endpoints &= bounded(black, width, height, black_donor) && black.color == black_donor;
                report("Black donors and black/white receivers retain authored endpoints", endpoints);
            }
        }
        {
            constexpr unsigned width = 64, height = 48;
            auto params = perspective(width, height); params.strength = 0;
            params.bounce[0] = 0.3f; params.bounce[1] = 4;
            const auto colors = corner_colors(width, height, Pixel{120,120,120,0}, Pixel{220,25,25,0});
            bool valid = true;
            auto depth = std::vector<float>(width * height, depth_at(10));
            for (unsigned shape = 0; shape < 3; ++shape) {
                if (shape == 1) for (unsigned y = height / 3; y < 2 * height / 3; ++y)
                    for (unsigned x = width / 3; x < 2 * width / 3; ++x) depth[y * width + x] = depth_at(8.5f);
                if (shape == 2) std::fill(depth.begin(), depth.end(), 1.0f);
                const auto output = fixture.run(width, height, depth, params, {}, colors);
                valid &= bounded(output, width, height, colors) && output.color == colors;
                for (const auto& value : output.raw_bounce)
                    valid &= value[0] == 0 && value[1] == 0 && value[2] == 0;
            }
            report("Coplanar colors, parallel raised faces, and background cannot self-illuminate", valid);
        }
        {
            constexpr unsigned width = 64, height = 48, half_width = width / 2, half_height = height / 2;
            auto params = perspective(width, height); params.strength = 0;
            params.bounce[0] = 0.3f; params.bounce[1] = 4;
            std::vector<Field> ambient(half_width * half_height, Field{1,1,0.5f,0.5f});
            std::vector<Field> seed(ambient.size());
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x)
                    seed[y * half_width + x] = {(x + y) % 2 ? 0.6f : 0, 0, 0, 1};
            auto depth = std::vector<float>(width * height, depth_at(10));
            const auto smooth = fixture.run(width, height, depth, params, ambient, {}, seed);
            bool valid = bounded(smooth, width, height);
            if (valid) {
                valid &= variation(smooth.filtered_bounce, half_width, half_height) <
                         variation(smooth.raw_bounce, half_width, half_height) * 0.25f;
                float mean = 0;
                for (const auto& value : smooth.filtered_bounce) mean += value[0];
                valid &= std::abs(mean / smooth.filtered_bounce.size() - 0.3f) < 0.005f;
            }
            report("Bounce filtering reduces same-plane variance while retaining mean energy", valid);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = width / 2; x < width; ++x) depth[y * width + x] = depth_at(40);
            for (unsigned y = 0; y < half_height; ++y)
                for (unsigned x = 0; x < half_width; ++x)
                    seed[y * half_width + x] = {x < half_width / 2 ? 0.6f : 0, 0, 0, 1};
            const auto separated = fixture.run(width, height, depth, params, ambient, {}, seed);
            valid = bounded(separated, width, height);
            const auto colors = scene(width, height);
            if (valid) {
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = width / 2; x < width; ++x)
                        valid &= separated.color[y * width + x] == colors[y * width + x];
                for (size_t i = 0; i < seed.size(); ++i)
                    valid &= std::abs(separated.filtered_bounce[i][0] - seed[i][0]) < 0.001f;
            }
            report("Shared depth guidance rejects bounce filtering and upsampling across a silhouette", valid);
        }
        {
            bool valid = true;
            for (auto size : {std::array<unsigned,2>{1,1}, {3,5}, {33,25}}) {
                const auto [width, height] = size;
                auto params = perspective(width, height); params.strength = 0;
                params.bounce[0] = 0.3f; params.bounce[1] = 4;
                const auto output = fixture.run(width, height,
                    std::vector<float>(width * height, depth_at(10)), params);
                valid &= bounded(output, width, height) && output.color == scene(width, height);
            }
            report("Active bounce remains finite and neutral on small and odd flat targets", valid);
        }
        {
            const auto shafts = [](unsigned width, unsigned height) {
                auto p = perspective(width, height); p.strength = 0;
                p.sun_screen[0] = p.sun_screen[1] = 0.5f;
                p.sun_screen[2] = 0.1f; p.sun_screen[3] = float(width) / height;
                p.sun_color[0] = 1; p.sun_color[1] = 0.4f; p.sun_color[2] = 0.1f;
                return p;
            };
            const auto colors = [](unsigned width, unsigned height) {
                auto pixels = scene(width, height);
                for (auto& pixel : pixels) pixel[0] = pixel[1] = pixel[2] = 100;
                return pixels;
            };
            for (auto size : {std::array<unsigned,2>{96,80}, {65,49}, {1,1}, {3,5}}) {
                const auto [width, height] = size;
                auto p = shafts(width, height);
                const auto pixels = colors(width, height);
                const std::vector<float> sky(width * height, 1);
                const auto output = fixture.run(width, height, sky, p, {}, pixels);
                bool valid = bounded(output, width, height, pixels);
                unsigned lifted = 0;
                if (valid) for (size_t i = 0; i < pixels.size(); ++i) {
                    lifted += output.color[i][0] > pixels[i][0];
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= output.color[i][c] >= pixels[i][c] &&
                            output.color[i][c] <= sunlight_code(pixels[i][c], p.sun_color[c], 0.1f) + 1;
                }
                if (width > 24 && valid) {
                    const auto& center = output.color[(height / 2) * width + width / 2];
                    valid &= lifted > 0 && center[0] > center[1] && center[1] > center[2];
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= std::abs(int(center[c]) - sunlight_code(100, p.sun_color[c], 0.1f)) <= 1;
                }
                char label[128];
                std::snprintf(label, sizeof(label), "Default ten-percent sunlight sky, linear headroom and exact alpha %ux%u lifted=%u", width, height, lifted);
                report(label, valid);
                if (width != 96) continue;
                auto blue = p; blue.sun_color[0] = 0.1f; blue.sun_color[2] = 1;
                const auto cool = fixture.run(width, height, sky, blue, {}, pixels);
                valid = bounded(cool, width, height, pixels);
                const size_t middle = (height / 2) * width + width / 2;
                if (valid) valid &= cool.color[middle][2] > cool.color[middle][0] &&
                    cool.color[middle][2] > output.color[middle][2];
                report("Sunlight contribution follows the supplied native source hue", valid);

                auto masked = sky;
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = width * 2 / 5; x < width * 9 / 20; ++x)
                        masked[y * width + x] = depth_at(10);
                const auto occluded = fixture.run(width, height, masked, p, {}, pixels);
                const size_t receiver = (height / 2) * width + width / 4;
                valid = bounded(occluded, width, height, pixels) && masked[receiver] == sky[receiver];
                if (valid) valid &= occluded.color[receiver][0] < output.color[receiver][0];
                report("Opaque radial mask decreases shafts at an unchanged sky receiver depth", valid);
                const auto opaque = fixture.run(width, height,
                    std::vector<float>(width * height, depth_at(10)), p, {}, pixels);
                report("All-opaque visibility produces exact source-color bypass",
                       bounded(opaque, width, height, pixels) && opaque.color == pixels);

                auto distant_params = p;
                constexpr float far = 10000;
                distant_params.proj_from_view[10] = distant_params.proj_from_view[14] = -far / (far - 1);
                distant_params.view_from_proj[11] = -(far - 1) / far;
                const auto depth = [](float distance) { return far / (far - 1) - (far / (far - 1)) / distance; };
                auto nearby_depth = sky, distant_depth = sky;
                nearby_depth[receiver] = depth(10); distant_depth[receiver] = depth(5000);
                const auto nearby = fixture.run(width, height, nearby_depth, distant_params, {}, pixels);
                const auto distant = fixture.run(width, height, distant_depth, distant_params, {}, pixels);
                valid = bounded(nearby, width, height, pixels) && bounded(distant, width, height, pixels);
                if (valid) valid &= distant.color[receiver][0] > nearby.color[receiver][0] &&
                    nearby.color[receiver][0] <= pixels[receiver][0] + 1;
                report("Equal radial visibility suppresses near-foreground veiling relative to distant receivers", valid);

                auto legacy = p; legacy.sun_screen[2] = 0; legacy.strength = 0.8f;
                legacy.fog_color[0] = 0.2f; legacy.fog_color[1] = 0.3f; legacy.fog_color[2] = 0.4f;
                legacy.fog_color[3] = 0.3f; legacy.fog_range[0] = 2; legacy.fog_range[1] = 18;
                const auto legacy_depth = corner_depth(width, height);
                const auto reference = fixture.run(width, height, legacy_depth, legacy, {}, pixels);
                legacy.sun_screen[0] = -1; legacy.sun_screen[1] = 2;
                const auto zero = fixture.run(width, height, legacy_depth, legacy, {}, pixels);
                report("Zero shaft strength preserves existing AO and fog color/fields exactly",
                       bounded(zero, width, height, pixels) && reference.color == zero.color &&
                       reference.raw == zero.raw && reference.filtered == zero.filtered);

                bool bypass = true;
                for (unsigned invalid = 0; invalid < 4; ++invalid) {
                    auto invalid_sun = p;
                    if (invalid == 0) invalid_sun.sun_screen[0] = 0;
                    if (invalid == 1) invalid_sun.sun_screen[1] = 1.01f;
                    if (invalid == 2) invalid_sun.sun_screen[3] = 0;
                    if (invalid == 3) invalid_sun.sun_color[0] = std::numeric_limits<float>::quiet_NaN();
                    const auto bad = fixture.run(width, height, sky, invalid_sun, {}, pixels);
                    bypass &= bounded(bad, width, height, pixels) && bad.color == pixels;
                }
                auto invalid_depth = sky; invalid_depth[receiver] = std::numeric_limits<float>::quiet_NaN();
                const auto bad_depth = fixture.run(width, height, invalid_depth, p, {}, pixels);
                bypass &= bounded(bad_depth, width, height, pixels);
                if (bad_depth.color.size() == pixels.size()) bypass &= bad_depth.color[receiver] == pixels[receiver];
                report("Invalid/offscreen sunlight and invalid receiver depth bypass without alpha corruption", bypass);

                auto composition = distant_params; composition.strength = 0.8f; composition.radius = 800;
                composition.fog_color[0] = 0.2f; composition.fog_color[1] = 0.3f; composition.fog_color[2] = 0.4f;
                composition.fog_color[3] = 0.3f; composition.fog_range[0] = 500; composition.fog_range[1] = 6000;
                auto geometry = sky;
                // Put the opaque horizon below the sun. With a horizon at
                // exactly sun_screen.y, every finite radial tap stays opaque.
                for (unsigned y = height * 3 / 5; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) geometry[y * width + x] = depth(2000);
                auto composition_off = composition; composition_off.sun_screen[2] = 0;
                const std::vector<Field> ambient_seed(((width + 1) / 2) * ((height + 1) / 2), Field{0.5f,1,0.5f,0.5f});
                const auto before = fixture.run(width, height, geometry, composition_off, ambient_seed, pixels);
                const auto after = fixture.run(width, height, geometry, composition, ambient_seed, pixels);
                valid = bounded(before, width, height, pixels) && bounded(after, width, height, pixels);
                unsigned distant_lifted = 0, bound_failures = 0;
                if (valid) for (size_t i = 0; i < pixels.size(); ++i) {
                    if (geometry[i] != 1) distant_lifted += after.color[i][0] > before.color[i][0];
                    for (unsigned c = 0; c < 3; ++c) {
                        const bool within_bound = int(after.color[i][c]) >= int(before.color[i][c]) - 1 &&
                            after.color[i][c] <= sunlight_code(before.color[i][c], p.sun_color[c], 0.1f) + 1;
                        bound_failures += !within_bound;
                        valid &= within_bound;
                    }
                }
                valid &= distant_lifted > 0;
                std::printf("Sunlight composition diagnostic: distant_lifted=%u bound_failures=%u\n",
                            distant_lifted, bound_failures);
                report("Sunlight composes over AO/fog inside a bounded linear headroom envelope", valid);
                auto reversed = composition;
                reversed.background_depth = 0; reversed.proj_from_view[10] = 1 / (far - 1);
                reversed.proj_from_view[14] = far / (far - 1);
                reversed.view_from_proj[11] = (far - 1) / far; reversed.view_from_proj[15] = 1 / far;
                auto reversed_depth = geometry;
                for (auto& value : reversed_depth) value = 1 - value;
                const auto reverse = fixture.run(width, height, reversed_depth, reversed, ambient_seed, pixels);
                valid = bounded(reverse, width, height, pixels) && reverse.color.size() == after.color.size();
                if (valid) for (size_t i = 0; i < after.color.size(); ++i)
                    for (unsigned c = 0; c < 3; ++c)
                        valid &= std::abs(int(reverse.color[i][c]) - int(after.color[i][c])) <= 1;
                report("Matched reversed-Z sunlight geometry and alpha agree within one display code", valid);
            }
        }
    }
    std::printf("%u ambient pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
    wgpuDeviceRelease(device); wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
