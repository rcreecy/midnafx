#include "midnafx_shader.hpp"
#include <webgpu/webgpu.h>

#include <cstdio>
#include <thread>

namespace {
struct AdapterResult {
    WGPUAdapter adapter = nullptr;
};
struct DeviceResult {
    WGPUDevice device = nullptr;
};
struct CompilationResult {
    bool complete = false;
    bool valid = true;
};
struct ScopeResult {
    bool complete = false;
    bool valid = false;
};
void adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView,
                      void* user, void*) {
    if (status == WGPURequestAdapterStatus_Success)
        static_cast<AdapterResult*>(user)->adapter = adapter;
}
void device_callback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* user,
                     void*) {
    if (status == WGPURequestDeviceStatus_Success)
        static_cast<DeviceResult*>(user)->device = device;
}
void compilation_callback(WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* info,
                          void* user, void*) {
    auto& result = *static_cast<CompilationResult*>(user);
    result.valid = status == WGPUCompilationInfoRequestStatus_Success && info != nullptr;
    if (info)
        for (size_t i = 0; i < info->messageCount; ++i) {
            const auto& message = info->messages[i];
            if (message.type == WGPUCompilationMessageType_Error)
                result.valid = false;
            std::fprintf(stderr, "WGSL line %llu: %.*s\n",
                         static_cast<unsigned long long>(message.lineNum),
                         static_cast<int>(message.message.length), message.message.data);
        }
    result.complete = true;
}
void scope_callback(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message,
                    void* user, void*) {
    auto& result = *static_cast<ScopeResult*>(user);
    result.complete = true;
    result.valid = status == WGPUPopErrorScopeStatus_Success && type == WGPUErrorType_NoError;
    if (!result.valid)
        std::fprintf(stderr, "WebGPU validation: %.*s\n", static_cast<int>(message.length),
                     message.data);
}
bool wait(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo wait_info = WGPU_FUTURE_WAIT_INFO_INIT;
    wait_info.future = future;
    while (!wait_info.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &wait_info, 0);
        if (status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut)
            return false;
        if (!wait_info.completed)
            std::this_thread::yield();
    }
    return true;
}
bool validate(WGPUInstance instance, WGPUDevice device, const char* source, const char* label,
              const char* const* entry_points, size_t entry_count) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {source, WGPU_STRLEN};
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &wgsl.chain;
    desc.label = {label, WGPU_STRLEN};
    auto shader = wgpuDeviceCreateShaderModule(device, &desc);
    if (!shader)
        return false;
    CompilationResult result;
    WGPUCompilationInfoCallbackInfo callback = WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_WaitAnyOnly;
    callback.callback = compilation_callback;
    callback.userdata1 = &result;
    const bool waited = wait(instance, wgpuShaderModuleGetCompilationInfo(shader, callback));
    bool valid = waited && result.complete && result.valid;
    for (size_t i = 0; valid && i < entry_count; ++i) {
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT;
        color.format = WGPUTextureFormat_RGBA8Unorm;
        color.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shader;
        fragment.entryPoint = {entry_points[i], WGPU_STRLEN};
        fragment.targetCount = 1;
        fragment.targets = &color;
        WGPURenderPipelineDescriptor pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pipeline_desc.vertex.module = shader;
        pipeline_desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
        pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pipeline_desc.multisample.count = 1;
        pipeline_desc.fragment = &fragment;
        auto pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeline_desc);
        ScopeResult scope;
        WGPUPopErrorScopeCallbackInfo scope_info = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        scope_info.mode = WGPUCallbackMode_WaitAnyOnly;
        scope_info.callback = scope_callback;
        scope_info.userdata1 = &scope;
        const bool scope_waited = wait(instance, wgpuDevicePopErrorScope(device, scope_info));
        valid = pipeline != nullptr && scope_waited && scope.complete && scope.valid;
        if (!valid)
            std::fprintf(stderr, "Pipeline entry point failed: %s\n", entry_points[i]);
        if (pipeline)
            wgpuRenderPipelineRelease(pipeline);
    }
    wgpuShaderModuleRelease(shader);
    return valid;
}
} // namespace

int main() {
    auto instance = wgpuCreateInstance(nullptr);
    if (!instance)
        return 77;
    AdapterResult adapter;
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.backendType = WGPUBackendType_Null;
    WGPURequestAdapterCallbackInfo adapter_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapter_info.mode = WGPUCallbackMode_WaitAnyOnly;
    adapter_info.callback = adapter_callback;
    adapter_info.userdata1 = &adapter;
    if (!wait(instance, wgpuInstanceRequestAdapter(instance, &options, adapter_info)) ||
        !adapter.adapter) {
        wgpuInstanceRelease(instance);
        return 77;
    }
    DeviceResult device;
    WGPURequestDeviceCallbackInfo device_info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    device_info.mode = WGPUCallbackMode_WaitAnyOnly;
    device_info.callback = device_callback;
    device_info.userdata1 = &device;
    if (!wait(instance, wgpuAdapterRequestDevice(adapter.adapter, nullptr, device_info)) ||
        !device.device) {
        wgpuAdapterRelease(adapter.adapter);
        wgpuInstanceRelease(instance);
        return 77;
    }
    constexpr const char* pass_entries[]{"fs_main"};
    constexpr const char* grade_entries[]{"fs_main", "fs_detail", "fs_debug", "fs_debug_detail"};
    constexpr const char* depth_entries[]{"fs_depth"};
    constexpr const char* dof_entries[]{"fs_coc"};
    const bool pass = validate(instance, device.device, midnafx::render::passthrough_shader,
                               "MidnaFX passthrough", pass_entries, 1);
    const bool grade = validate(instance, device.device, midnafx::render::grading_shader,
                                "MidnaFX grading/detail/debug", grade_entries, 4);
    const bool depth = validate(instance, device.device,
                                midnafx::render::atmosphere_depth_shader,
                                "MidnaFX atmosphere depth diagnostic", depth_entries, 1);
    const bool dof = validate(instance, device.device, midnafx::render::dof_shader,
                              "MidnaFX depth of field diagnostic", dof_entries, 1);
    wgpuDeviceRelease(device.device);
    wgpuAdapterRelease(adapter.adapter);
    wgpuInstanceRelease(instance);
    return pass && grade && depth && dof ? 0 : 1;
}
