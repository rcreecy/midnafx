#pragma once
#include <webgpu/webgpu.h>
#include <thread>

namespace midnafx::render::gpu {
// Resource creation may return non-null error objects. Consume every scope before
// publishing handles or falling back to the original scene.
class ValidationScopes {
    WGPUDevice device_;
    WGPUInstance instance_;
    bool finished_ = false;
    bool valid_ = true;
public:
    ValidationScopes(WGPUDevice device, WGPUInstance instance) : device_(device), instance_(instance) {
        wgpuDevicePushErrorScope(device_, WGPUErrorFilter_OutOfMemory);
        wgpuDevicePushErrorScope(device_, WGPUErrorFilter_Internal);
        wgpuDevicePushErrorScope(device_, WGPUErrorFilter_Validation);
    }
    ValidationScopes(const ValidationScopes&) = delete;
    ValidationScopes& operator=(const ValidationScopes&) = delete;
    ~ValidationScopes() { finish(); }
    bool finish() {
        if (finished_) return valid_;
        finished_ = true;
        for (unsigned i = 0; i < 3; ++i) {
            bool ok = false;
            WGPUPopErrorScopeCallbackInfo callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
            callback.mode = WGPUCallbackMode_WaitAnyOnly;
            callback.userdata1 = &ok;
            callback.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type,
                                   WGPUStringView, void* user, void*) {
                *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success &&
                                           type == WGPUErrorType_NoError;
            };
            WGPUFutureWaitInfo wait = WGPU_FUTURE_WAIT_INFO_INIT;
            wait.future = wgpuDevicePopErrorScope(device_, callback);
            while (!wait.completed) {
                const auto status = wgpuInstanceWaitAny(instance_, 1, &wait, 0);
                if (status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) break;
                if (!wait.completed) std::this_thread::yield();
            }
            valid_ &= wait.completed && ok;
        }
        return valid_;
    }
};
} // namespace midnafx::render::gpu
