#include "midnafx_shader.hpp"
#include <webgpu/webgpu.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {
using Pixel = std::array<unsigned char, 4>;
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
                          unsigned width, unsigned height, const std::vector<Pixel>& pixels, float detail_strength = 0.0f) {
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    auto queue = wgpuDeviceGetQueue(device);
    WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
    td.size = {width, height, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
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

    // Production Grading layout, with neutral tone controls and selectable detail.
    const std::array<float, 12> neutral{1, 1, 1, 0, 1, 1, 1, 0, detail_strength, 1, 0, 0};
    WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
    bd.size = sizeof(neutral);
    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    auto uniform = wgpuDeviceCreateBuffer(device, &bd);
    wgpuQueueWriteBuffer(queue, uniform, 0, neutral.data(), sizeof(neutral));
    const unsigned stride = (width * 4 + 255) & ~255u;
    bd.size = stride * height;
    bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    auto readback = wgpuDeviceCreateBuffer(device, &bd);

    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {midnafx::render::grading_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor sd = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    sd.nextInChain = &wgsl.chain;
    auto shader = wgpuDeviceCreateShaderModule(device, &sd);
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
    color.format = td.format;
    color.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {entry, WGPU_STRLEN};
    fragment.targetCount = 1;
    fragment.targets = &color;
    WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pd.vertex.module = shader;
    pd.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pd.fragment = &fragment;
    auto pipeline = wgpuDeviceCreateRenderPipeline(device, &pd);
    auto layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    WGPUBindGroupEntry entries[2] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = source_view;
    entries[1].binding = 1;
    entries[1].buffer = uniform;
    entries[1].size = sizeof(neutral);
    WGPUBindGroupDescriptor bgd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bgd.layout = layout;
    bgd.entryCount = 2;
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
    wgpuTextureViewRelease(target_view);
    wgpuTextureViewRelease(source_view);
    wgpuTextureRelease(target);
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
            ok &= valid;
            ++cases;
        }
    }
    std::printf("%u pixel cases: %s\n", cases, ok ? "PASS" : "FAIL");
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    return ok ? 0 : 1;
}
