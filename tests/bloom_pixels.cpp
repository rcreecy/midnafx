#include "midnafx_shader.hpp"
#include "render/bloom_plan.hpp"
#include <webgpu/webgpu.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>

namespace {
using Pixel = std::array<unsigned char, 4>;
using Field = std::array<float, 4>;
struct Controls { float strength, blur_scale; float padding[2]{}; };
static_assert(sizeof(Controls) == 16);

void wait(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!info.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &info, 0);
        if ((status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) ||
            std::chrono::steady_clock::now() > deadline) {
            std::fprintf(stderr, "Bloom GPU callback failed or timed out\n");
            std::exit(1);
        }
        if (!info.completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
float decode_half(std::uint16_t value) {
    const unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
    if (exponent == 31) return NAN;
    const float magnitude = exponent ? std::ldexp(float(1024 + fraction), int(exponent) - 25) :
                                       std::ldexp(float(fraction), -24);
    return value & 0x8000 ? -magnitude : magnitude;
}
std::vector<Pixel> uniform_scene(unsigned width, unsigned height, Pixel color) {
    std::vector<Pixel> pixels(width * height, color);
    for (unsigned i = 0; i < pixels.size(); ++i) pixels[i][3] = static_cast<unsigned char>((i * 29 + 7) % 256);
    return pixels;
}
struct Output {
    std::vector<Pixel> color;
    std::vector<Field> raw, filtered;
};
struct Fixture {
    WGPUInstance instance;
    WGPUDevice device;
    WGPUQueue queue;
    std::array<WGPUShaderModule, 2> shaders{};
    std::array<WGPUComputePipeline, 5> compute{};
    std::array<WGPUBindGroupLayout, 5> layouts{};
    WGPURenderPipeline composite;
    WGPUBindGroupLayout composite_layout;
    WGPUSampler sampler;

    Fixture(WGPUInstance instance, WGPUDevice device) : instance(instance), device(device) {
        queue = wgpuDeviceGetQueue(device);
        const char* sources[]{midnafx::render::bloom_compute_shader, midnafx::render::grading_shader};
        for (unsigned i = 0; i < 2; ++i) {
            WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
            source.code = {sources[i], WGPU_STRLEN};
            WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
            desc.nextInChain = &source.chain;
            shaders[i] = wgpuDeviceCreateShaderModule(device, &desc);
        }
        const char* names[]{"cs_extract", "cs_blur_horizontal", "cs_blur_vertical", "cs_downsample", "cs_upsample"};
        for (unsigned i = 0; i < 5; ++i) {
            WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
            desc.compute.module = shaders[0]; desc.compute.entryPoint = {names[i], WGPU_STRLEN};
            compute[i] = wgpuDeviceCreateComputePipeline(device, &desc);
            layouts[i] = wgpuComputePipelineGetBindGroupLayout(compute[i], 0);
        }
        WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
        color.format = WGPUTextureFormat_RGBA8Unorm; color.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shaders[1]; fragment.entryPoint = {"fs_main", WGPU_STRLEN};
        fragment.targetCount = 1; fragment.targets = &color;
        WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        desc.vertex.module = shaders[1]; desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
        desc.fragment = &fragment;
        composite = wgpuDeviceCreateRenderPipeline(device, &desc);
        composite_layout = wgpuRenderPipelineGetBindGroupLayout(composite, 0);
        WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
        sampler_desc.addressModeU = sampler_desc.addressModeV = sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler_desc.magFilter = sampler_desc.minFilter = WGPUFilterMode_Linear;
        sampler = wgpuDeviceCreateSampler(device, &sampler_desc);
    }
    ~Fixture() {
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(composite_layout); wgpuRenderPipelineRelease(composite);
        for (auto layout : layouts) wgpuBindGroupLayoutRelease(layout);
        for (auto pipeline : compute) wgpuComputePipelineRelease(pipeline);
        for (auto shader : shaders) wgpuShaderModuleRelease(shader);
        wgpuQueueRelease(queue);
    }

    Output run(unsigned width, unsigned height, const std::vector<Pixel>& pixels, float strength = 0.5f, bool multiscale = true) {
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        auto plan = midnafx::render::bloom::make_plan(width, height);
        if (!multiscale) plan.count = 1;
        const unsigned quarter_width = plan.levels[0].width, quarter_height = plan.levels[0].height;
        const unsigned exposure_index = 2 + 2 * plan.count;
        const unsigned result_index = plan.count > 1 ? 3 : 2;
        const unsigned color_stride = (width * 4 + 255) & ~255u;
        const unsigned field_stride = (quarter_width * 8 + 255) & ~255u;
        const size_t raw_offset = size_t(color_stride) * height;
        const size_t filtered_offset = raw_offset + size_t(field_stride) * quarter_height;
        std::vector<WGPUTexture> textures(exposure_index + 1);
        std::vector<WGPUTextureView> views(exposure_index + 1);
        const auto texture = [&](unsigned index, unsigned w, unsigned h, WGPUTextureFormat format, WGPUTextureUsage usage) {
            WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
            desc.size = {w, h, 1}; desc.format = format; desc.usage = usage;
            textures[index] = wgpuDeviceCreateTexture(device, &desc);
            views[index] = wgpuTextureCreateView(textures[index], nullptr);
        };
        texture(0, width, height, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
        texture(1, width, height, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc);
        for (unsigned level = 0; level < plan.count; ++level)
            for (unsigned side = 0; side < 2; ++side)
                texture(2 + 2 * level + side, plan.levels[level].width, plan.levels[level].height,
                    WGPUTextureFormat_RGBA16Float,
                    WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc);
        // WebGPU initializes this exposure texture to zero EV.
        texture(exposure_index, 1, 1, WGPUTextureFormat_RGBA16Float, WGPUTextureUsage_TextureBinding);
        WGPUTexelCopyTextureInfo upload = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        upload.texture = textures[0];
        WGPUTexelCopyBufferLayout upload_layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
        upload_layout.bytesPerRow = width * 4; upload_layout.rowsPerImage = height;
        const WGPUExtent3D full_size{width, height, 1};
        wgpuQueueWriteTexture(queue, &upload, pixels.data(), pixels.size() * sizeof(Pixel), &upload_layout, &full_size);
        WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
        bd.size = sizeof(Controls); bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        auto uniform = wgpuDeviceCreateBuffer(device, &bd);
        const Controls controls{strength, 1};
        wgpuQueueWriteBuffer(queue, uniform, 0, &controls, sizeof(controls));
        const std::array<float, 12> neutral{1,1,1,0,1,1,1,0,0,1,0,0};
        bd.size = sizeof(neutral);
        auto grade = wgpuDeviceCreateBuffer(device, &bd);
        wgpuQueueWriteBuffer(queue, grade, 0, neutral.data(), sizeof(neutral));
        bd.size = filtered_offset + size_t(field_stride) * quarter_height;
        bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        auto readback = wgpuDeviceCreateBuffer(device, &bd);
        struct Job { WGPUBindGroup group; unsigned pipeline, width, height; };
        std::vector<Job> jobs;
        const auto add = [&](unsigned pipeline, unsigned level, unsigned input, unsigned output,
                             unsigned fine = 0) {
            WGPUBindGroupEntry entries[5];
            for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
            unsigned count = 0;
            entries[count].binding = pipeline == 0 ? 0 : 1;
            entries[count++].textureView = views[input];
            entries[count].binding = 2; entries[count++].textureView = views[output];
            if (pipeline == 1 || pipeline == 2 || pipeline == 4) {
                entries[count].binding = 3; entries[count++].sampler = sampler;
            }
            if (pipeline < 3) {
                entries[count].binding = 4; entries[count].buffer = uniform;
                entries[count++].size = sizeof(controls);
            }
            if (fine) { entries[count].binding = 5; entries[count++].textureView = views[fine]; }
            WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
            desc.layout = layouts[pipeline]; desc.entryCount = count; desc.entries = entries;
            jobs.push_back({wgpuDeviceCreateBindGroup(device, &desc), pipeline,
                            plan.levels[level].width, plan.levels[level].height});
        };
        add(0, 0, 0, 2); add(1, 0, 2, 3); add(2, 0, 3, 2);
        for (unsigned level = 1; level < plan.count; ++level)
            add(3, level, 2 + 2 * (level - 1), 2 + 2 * level);
        for (unsigned level = plan.count - 1; level > 0; --level)
            add(4, level - 1, 2 + 2 * level + (level + 1 == plan.count ? 0 : 1),
                3 + 2 * (level - 1), 2 + 2 * (level - 1));
        WGPUBindGroupEntry entries[5];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entries[0].binding = 0; entries[0].textureView = views[0];
        entries[1].binding = 1; entries[1].buffer = grade; entries[1].size = sizeof(neutral);
        entries[2].binding = 10; entries[2].textureView = views[exposure_index];
        entries[3].binding = 11; entries[3].textureView = views[result_index];
        entries[4].binding = 12; entries[4].sampler = sampler;
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = composite_layout; desc.entryCount = 5; desc.entries = entries;
        auto group = wgpuDeviceCreateBindGroup(device, &desc);
        auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
        const auto dispatch = [&](unsigned index) {
            auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
            const auto& job = jobs[index];
            wgpuComputePassEncoderSetPipeline(pass, compute[job.pipeline]);
            wgpuComputePassEncoderSetBindGroup(pass, 0, job.group, 0, nullptr);
            wgpuComputePassEncoderDispatchWorkgroups(pass, (job.width + 7) / 8, (job.height + 7) / 8, 1);
            wgpuComputePassEncoderEnd(pass); wgpuComputePassEncoderRelease(pass);
        };
        const auto copy = [&](WGPUTexture source_texture, size_t offset, unsigned stride, unsigned w, unsigned h) {
            WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            source.texture = source_texture;
            WGPUTexelCopyBufferInfo dest = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
            dest.buffer = readback; dest.layout.offset = offset; dest.layout.bytesPerRow = stride; dest.layout.rowsPerImage = h;
            const WGPUExtent3D extent{w, h, 1};
            wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &dest, &extent);
        };
        dispatch(0); copy(textures[2], raw_offset, field_stride, quarter_width, quarter_height);
        for (unsigned i = 1; i < jobs.size(); ++i) dispatch(i);
        copy(textures[result_index], filtered_offset, field_stride, quarter_width, quarter_height);
        WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        attachment.view = views[1]; attachment.loadOp = WGPULoadOp_Clear; attachment.storeOp = WGPUStoreOp_Store;
        WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        pass_desc.colorAttachmentCount = 1; pass_desc.colorAttachments = &attachment;
        auto pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
        wgpuRenderPassEncoderSetPipeline(pass, composite); wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0); wgpuRenderPassEncoderEnd(pass);
        copy(textures[1], 0, color_stride, width, height);
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
                    result->resize(quarter_width * quarter_height);
                    for (unsigned y = 0; y < quarter_height; ++y)
                        for (unsigned x = 0; x < quarter_width; ++x) {
                            std::array<std::uint16_t, 4> value{};
                            std::memcpy(value.data(), data + offset + y * field_stride + x * 8, 8);
                            for (unsigned c = 0; c < 4; ++c) (*result)[y * quarter_width + x][c] = decode_half(value[c]);
                        }
                }
            }
            wgpuBufferUnmap(readback);
        }
        bool valid = false;
        WGPUPopErrorScopeCallbackInfo scope = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        scope.mode = WGPUCallbackMode_WaitAnyOnly; scope.userdata1 = &valid;
        scope.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* user, void*) {
            *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success && type == WGPUErrorType_NoError;
            if (!*static_cast<bool*>(user)) std::fprintf(stderr, "Bloom validation: %.*s\n", int(message.length), message.data);
        };
        wait(instance, wgpuDevicePopErrorScope(device, scope));
        if (!valid) output = {};
        wgpuCommandBufferRelease(command); wgpuRenderPassEncoderRelease(pass); wgpuCommandEncoderRelease(encoder);
        wgpuBindGroupRelease(group); for (auto job : jobs) wgpuBindGroupRelease(job.group);
        wgpuBufferRelease(readback); wgpuBufferRelease(grade); wgpuBufferRelease(uniform);
        for (auto view : views) wgpuTextureViewRelease(view);
        for (auto item : textures) wgpuTextureRelease(item);
        return output;
    }
};

bool bounded(const Output& output, unsigned width, unsigned height, const std::vector<Pixel>& source) {
    if (output.color.size() != source.size() || output.raw.size() != ((width + 3) / 4) * ((height + 3) / 4) ||
        output.filtered.size() != output.raw.size()) return false;
    for (size_t i = 0; i < source.size(); ++i) {
        if (output.color[i][3] != source[i][3]) return false;
        for (unsigned c = 0; c < 3; ++c) if (output.color[i][c] < source[i][c]) return false;
    }
    for (const auto* field : {&output.raw, &output.filtered})
        for (const auto& value : *field) {
            for (float channel : value) if (!std::isfinite(channel) || channel < 0 || channel > 0.176f) return false;
            if (value[3] != 0) return false;
        }
    return true;
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
        bool valid = true;
        for (auto color : {Pixel{0,0,0,0}, Pixel{100,120,140,0}}) {
            const auto source = uniform_scene(33, 25, color);
            const auto output = fixture.run(33, 25, source);
            valid &= bounded(output, 33, 25, source) && output.color == source;
            for (const auto& value : output.filtered) valid &= value == Field{0,0,0,0};
        }
        report("Black and dim scenes remain exact", valid);
        {
            const auto source = uniform_scene(31, 19, {255,255,255,0});
            const auto output = fixture.run(31, 19, source);
            valid = bounded(output, 31, 19, source) && output.color == source;
            for (const auto& value : output.raw) valid &= std::abs(value[0] - 0.175f) < 0.0002f;
            report("White endpoint and alpha survive active bloom", valid);
        }
        auto point = uniform_scene(64, 48, {0,0,0,0});
        for (unsigned y = 20; y < 24; ++y)
            for (unsigned x = 8; x < 12; ++x) point[y * 64 + x] = {255,255,255,point[y * 64 + x][3]};
        // A four-pixel-wide highlight remains visible after LDR quantization at
        // the default 20% strength; a lone source texel need not do so.
        const auto spread = fixture.run(64, 48, point, 0.2f);
        valid = bounded(spread, 64, 48, point);
        unsigned glow_pixels = 0;
        if (valid) {
            for (unsigned y = 0; y < 48; ++y)
                for (unsigned x = 0; x < 64; ++x) {
                    const auto i = y * 64 + x;
                    glow_pixels += point[i][0] == 0 && spread.color[i][0] > 0;
                    if (x >= 48) valid &= spread.color[i] == point[i];
                }
            valid &= glow_pixels > 0 && spread.color[21 * 64 + 12][0] > 0;
            valid &= spread.filtered[5 * 16 + 3][0] > 0 && spread.filtered[5 * 16 + 15][0] == 0;
        }
        report("Small bright feature spreads locally without wrapping", valid);
        {
            auto red = point;
            for (auto& pixel : red) pixel[1] = pixel[2] = 0;
            const auto output = fixture.run(64, 48, red, 0.2f);
            valid = bounded(output, 64, 48, red);
            if (valid) {
                valid &= output.color[21 * 64 + 12][0] > 0;
                for (const auto& pixel : output.color) valid &= pixel[1] == 0 && pixel[2] == 0;
            }
            report("Colored glow retains its source RGB channel", valid);
        }
        {
            auto source = point;
            for (unsigned i = 0; i < source.size(); ++i) source[i] = {
                static_cast<unsigned char>((i * 37) % 256), static_cast<unsigned char>((i * 73) % 256),
                static_cast<unsigned char>((i * 19) % 256), static_cast<unsigned char>(i % 256)};
            const auto output = fixture.run(64, 48, source, 0);
            valid = bounded(output, 64, 48, source) && output.color == source;
            for (const auto& value : output.filtered) valid &= value == Field{0,0,0,0};
            report("Zero strength is exact identity", valid);
        }
        {
            auto source = uniform_scene(7, 5, {0,0,0,0});
            source.back() = {255,255,255,source.back()[3]};
            const auto output = fixture.run(7, 5, source);
            valid = bounded(output, 7, 5, source);
            if (valid) for (size_t i = 0; i < output.raw.size(); ++i) {
                // The final quarter tile contains exactly three source pixels.
                const float expected = i + 1 == output.raw.size() ? 0.175f / 3 : 0;
                for (unsigned c = 0; c < 3; ++c) valid &= std::abs(output.raw[i][c] - expected) < 0.0001f;
            }
            const auto single = uniform_scene(1, 1, {255,255,255,0});
            const auto single_output = fixture.run(1, 1, single);
            valid &= bounded(single_output, 1, 1, single) && single_output.color == single;
            if (!single_output.filtered.empty()) valid &= std::abs(single_output.filtered[0][0] - 0.175f) < 0.0003f;
            report("Odd tiles and one pixel have finite, unduplicated edge energy", valid);
        }
        {
            float previous = 0;
            valid = true;
            for (unsigned code : {165u,166u,168u,174u}) {
                const auto source = uniform_scene(9, 7, {static_cast<unsigned char>(code),static_cast<unsigned char>(code),static_cast<unsigned char>(code),0});
                const auto output = fixture.run(9, 7, source);
                valid &= bounded(output, 9, 7, source);
                if (!output.raw.empty()) {
                    const float energy = output.raw[0][0];
                    valid &= energy >= previous && energy - previous < 0.001f;
                    if (code == 165) valid &= energy == 0;
                    if (code == 174) valid &= energy > 0;
                    previous = energy;
                }
            }
            report("Soft threshold opens continuously above dark-scene cutoff", valid);
        }
        {
            const auto source = uniform_scene(64, 48, {0,0,0,0});
            const auto output = fixture.run(64, 48, source);
            valid = bounded(output, 64, 48, source) && output.color == source;
            for (const auto& value : output.filtered) valid &= value == Field{0,0,0,0};
            report("Fresh black draw after bright inputs remains exact", valid);
        }
    }
    {
        Fixture fixture(instance, device);
        {
            bool valid = true;
            for (auto color : {Pixel{0,0,0,0}, Pixel{100,120,140,0}, Pixel{255,255,255,0}, Pixel{255,0,0,0}}) {
                const auto source = uniform_scene(513, 385, color);
                const auto output = fixture.run(513, 385, source);
                valid &= bounded(output, 513, 385, source);
                if (!output.filtered.empty()) for (const auto& field : output.filtered)
                    for (unsigned c = 0; c < 3; ++c) {
                        const float expected = color[c] == 255 ? 0.175f : 0;
                        valid &= std::abs(field[c] - expected) < 0.0004f;
                    }
                if (color[0] != 255 || color[1] == 255) valid &= output.color == source;
            }
            report("Multi-scale uniform fields retain strength, color, alpha and dark/white endpoints", valid);
        }
        auto patch = uniform_scene(960, 720, {12,12,12,0});
        for (unsigned y = 296; y < 424; ++y)
            for (unsigned x = 416; x < 544; ++x) patch[y * 960 + x] = {255,255,255,patch[y * 960 + x][3]};
        const auto core = fixture.run(960, 720, patch, 0.2f, false);
        const auto halo = fixture.run(960, 720, patch, 0.2f);
        if (const auto* directory = std::getenv("MIDNAFX_BLOOM_EVIDENCE_DIR")) {
            const std::filesystem::path path(directory);
            std::filesystem::create_directories(path);
            const auto save = [&](const char* name, const std::vector<Pixel>& pixels) {
                std::ofstream file(path / name, std::ios::binary);
                file << "P6\n960 720\n255\n";
                for (const auto& pixel : pixels) file.write(reinterpret_cast<const char*>(pixel.data()), 3);
            };
            if (core.color.size() == patch.size() && halo.color.size() == patch.size()) {
                save("source.ppm", patch); save("core.ppm", core.color); save("multiscale.ppm", halo.color);
                std::ofstream profile(path / "profile.csv");
                profile << "pixel_x,core_field,multiscale_field\n";
                for (unsigned x = 0; x < 240; ++x)
                    profile << x * 4 + 2 << ',' << core.filtered[90 * 240 + x][0] << ',' << halo.filtered[90 * 240 + x][0] << '\n';
            }
        }
        {
            bool valid = bounded(core, 960, 720, patch) && bounded(halo, 960, 720, patch);
            double core_energy = 0, halo_energy = 0, core_outer = 0, halo_outer = 0;
            unsigned visible_outer = 0;
            if (valid) {
                for (unsigned y = 0; y < 180; ++y) for (unsigned x = 0; x < 240; ++x) {
                    const auto i = y * 240 + x;
                    const float dx = std::max({416 - (float(x) * 4 + 2), (float(x) * 4 + 2) - 544, 0.0f});
                    const float dy = std::max({296 - (float(y) * 4 + 2), (float(y) * 4 + 2) - 424, 0.0f});
                    core_energy += core.filtered[i][0]; halo_energy += halo.filtered[i][0];
                    if (std::max(dx,dy) >= 24) {
                        core_outer += core.filtered[i][0]; halo_outer += halo.filtered[i][0];
                    }
                }
                for (unsigned y = 0; y < 720; ++y) for (unsigned x = 0; x < 960; ++x)
                    if (x < 392 || x >= 568 || y < 272 || y >= 448)
                        visible_outer += halo.color[y * 960 + x][0] > core.color[y * 960 + x][0];
                valid &= std::abs(halo_energy / core_energy - 1) < 0.01;
                valid &= halo_outer > core_outer * 2 && visible_outer > 0;
                valid &= halo.filtered[90 * 240 + 120][0] > core.filtered[90 * 240 + 120][0] * 0.6f;
            }
            std::printf("Halo comparison: core_energy=%.8f multi_energy=%.8f core_outer=%.8f multi_outer=%.8f visible_outer=%u\n",
                        core_energy, halo_energy, core_outer, halo_outer, visible_outer);
            report("Default strength gains a wider visible halo while retaining core and total interior energy", valid);
        }
        {
            bool valid = true;
            double first_energy = 0;
            for (unsigned shift = 0; shift < 4; ++shift) {
                auto source = uniform_scene(512, 384, {0,0,0,0});
                for (unsigned y = 188; y < 192; ++y) for (unsigned x = 252 + shift; x < 256 + shift; ++x)
                    source[y * 512 + x] = {255,0,0,source[y * 512 + x][3]};
                const auto output = fixture.run(512, 384, source);
                valid &= bounded(output, 512, 384, source);
                double energy = 0, moment_x = 0, moment_y = 0;
                for (unsigned y = 0; y < 96 && !output.filtered.empty(); ++y)
                    for (unsigned x = 0; x < 128; ++x) {
                        const auto& field = output.filtered[y * 128 + x];
                        energy += field[0]; moment_x += (x * 4 + 2) * field[0]; moment_y += (y * 4 + 2) * field[0];
                        valid &= field[1] == 0 && field[2] == 0;
                    }
                if (shift == 0) first_energy = energy;
                valid &= energy > 0 && std::abs(energy / first_energy - 1) < 0.01;
                valid &= std::abs(moment_x / energy - (254 + shift)) < 1.0;
                valid &= std::abs(moment_y / energy - 190) < 1.0;
                std::printf("Moving highlight: shift=%u energy=%.8f centroid=(%.5f,%.5f)\n",
                            shift, energy, moment_x / energy, moment_y / energy);
            }
            report("One-pixel motion across extraction phases retains small-highlight energy and centroid", valid);
        }
        {
            auto source = uniform_scene(513, 385, {0,0,0,0});
            for (unsigned y = 180; y < 204; ++y) for (unsigned x = 0; x < 16; ++x)
                source[y * 513 + x] = {255,0,0,source[y * 513 + x][3]};
            const auto output = fixture.run(513, 385, source);
            bool valid = bounded(output, 513, 385, source);
            if (valid) for (unsigned y = 0; y < 385; ++y) for (unsigned x = 400; x < 513; ++x)
                valid &= output.color[y * 513 + x] == source[y * 513 + x];
            report("Odd-size pyramid at a bright border remains bounded without opposite-edge wrap", valid);
        }
        {
            const auto source = uniform_scene(1, 2161, {255,255,255,0});
            const auto output = fixture.run(1, 2161, source);
            bool valid = bounded(output, 1, 2161, source) && output.color == source;
            if (valid) for (const auto& field : output.filtered) valid &= std::abs(field[0] - 0.175f) < 0.0004f;
            report("Five-level one-column odd pyramid retains finite uniform energy", valid);
        }
        {
            const auto double_strength = fixture.run(960, 720, patch, 0.4f);
            bool valid = bounded(double_strength, 960, 720, patch) && !halo.filtered.empty();
            if (valid) for (unsigned i = 0; i < halo.filtered.size(); ++i)
                valid &= std::abs(double_strength.filtered[i][0] - 2 * halo.filtered[i][0]) < 0.0001f;
            report("Existing strength scales the complete halo without multiplying level count", valid);
        }
    }
    std::printf("%u bloom pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
    wgpuDeviceRelease(device); wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
