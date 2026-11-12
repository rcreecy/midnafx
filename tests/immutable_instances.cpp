#include "render/immutable_instances.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace midnafx::render::immutable_instances;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "failed line %d\n", __LINE__); return false; } } while (false)
static void wait(WGPUInstance instance, WGPUFuture future) {
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT; info.future = future;
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!info.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &info, 0);
        if ((status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) ||
            std::chrono::steady_clock::now() > end) { std::fputs("GPU callback timeout\n", stderr); std::exit(1); }
        if (!info.completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static Resource* warm(Pool& pool, WGPUDevice device, std::span<const std::byte> bytes) {
    Resource* result = nullptr;
    for (unsigned frame = 0; frame < Pool::WarmFrames; ++frame) result = pool.prepare(device, bytes, true);
    return result;
}
static bool run(WGPUInstance instance, WGPUDevice device) {
    std::array<std::uint32_t, 4096> words{};
    for (unsigned i = 0; i < words.size(); ++i) words[i] = i * 2654435761u;
    const auto bytes = std::as_bytes(std::span(words));
    Pool pool(WGPUBufferUsage_CopySrc);
    CHECK(!pool.prepare(device, bytes, false));
    for (unsigned i = 1; i < Pool::WarmFrames; ++i) CHECK(!pool.prepare(device, bytes, true));
    auto* original = pool.prepare(device, bytes, true);
    CHECK(original && original->bytes == bytes.size() && pool.sets() == 1 && pool.bytes() == bytes.size());
    CHECK(pool.prepare(device, bytes, true) == original);
    pin(original); pin(original);
    pool.invalidate(); CHECK(pool.sets() == 1);
    release(original); pool.collect(); CHECK(pool.sets() == 1);

    // Encode before retiring the last owner. WebGPU must retain the source for
    // execution even after the callback's pin and pool reference have gone away.
    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.size = bytes.size(); desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    auto readback = wgpuDeviceCreateBuffer(device, &desc);
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, original->buffer, 0, readback, 0, bytes.size());
    release(original); pool.collect(); CHECK(pool.sets() == 0);
    auto command = wgpuCommandEncoderFinish(encoder, nullptr);
    auto queue = wgpuDeviceGetQueue(device);
    wgpuQueueSubmit(queue, 1, &command);
    bool mapped = false;
    WGPUBufferMapCallbackInfo map = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    map.mode = WGPUCallbackMode_WaitAnyOnly; map.userdata1 = &mapped;
    map.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUMapAsyncStatus_Success;
    };
    wait(instance, wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, bytes.size(), map));
    CHECK(mapped);
    const auto* result = wgpuBufferGetConstMappedRange(readback, 0, bytes.size());
    CHECK(result && std::memcmp(result, bytes.data(), bytes.size()) == 0);
    wgpuBufferUnmap(readback);
    wgpuBufferRelease(readback); wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(encoder); wgpuQueueRelease(queue);
    std::puts("Immutable GPU bytes and post-encoding retirement: PASS");

    // Distinct queued generations cannot reuse/overwrite a pinned snapshot.
    std::array<Resource*, Pool::MaxSets> pinned{};
    for (unsigned i = 0; i < pinned.size(); ++i) {
        pool.invalidate(); words[0] = i;
        pinned[i] = warm(pool, device, bytes); CHECK(pinned[i]); pin(pinned[i]);
        for (unsigned j = 0; j < i; ++j) CHECK(pinned[i] != pinned[j]);
    }
    pool.invalidate(); CHECK(!warm(pool, device, bytes) && pool.sets() == Pool::MaxSets);
    for (auto* value : pinned) release(value);
    pool.collect(); CHECK(pool.sets() == 0);
    CHECK(warm(pool, device, bytes));
    CHECK(!pool.prepare(device, bytes, false)); CHECK(pool.sets() == 0);
    CHECK(!pool.prepare(device, bytes.first(3), true));
    pool.invalidate();
    CHECK(!warm(pool, nullptr, bytes)); CHECK(pool.sets() == 0);
    std::vector<std::byte> too_large(Pool::MaxBytes + 4);
    CHECK(!warm(pool, device, too_large)); CHECK(pool.sets() == 0);
    CHECK(!pool.prepare(device, {}, true));
    CHECK(warm(pool, device, bytes));
    pool.clear_after_drain(); CHECK(pool.sets() == 0 && pool.bytes() == 0);
    Pool normal_usage;
    CHECK(warm(normal_usage, device, bytes));
    normal_usage.clear_after_drain();
    std::puts("Warmup, changing frames, backpressure, unsupported data, reload and shutdown: PASS");
    return true;
}
int main() {
    auto instance = wgpuCreateInstance(nullptr); if (!instance) return 77;
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
#if defined(_WIN32)
    options.backendType = WGPUBackendType_D3D12;
#elif defined(__APPLE__)
    options.backendType = WGPUBackendType_Metal;
#else
    options.backendType = WGPUBackendType_Vulkan;
#endif
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo ac = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    ac.mode = WGPUCallbackMode_WaitAnyOnly; ac.userdata1 = &adapter;
    ac.callback = [](WGPURequestAdapterStatus status, WGPUAdapter value, WGPUStringView, void* user, void*) {
        if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(user) = value;
    };
    wait(instance, wgpuInstanceRequestAdapter(instance, &options, ac));
    if (!adapter) { wgpuInstanceRelease(instance); return 77; }
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT; wgpuAdapterGetInfo(adapter, &info);
    std::printf("Adapter: %.*s backend=%u\n", int(info.device.length), info.device.data, unsigned(info.backendType));
    wgpuAdapterInfoFreeMembers(info);
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo dc = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    dc.mode = WGPUCallbackMode_WaitAnyOnly; dc.userdata1 = &device;
    dc.callback = [](WGPURequestDeviceStatus status, WGPUDevice value, WGPUStringView, void* user, void*) {
        if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(user) = value;
    };
    wait(instance, wgpuAdapterRequestDevice(adapter, nullptr, dc));
    if (!device) { wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance); return 1; }
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    bool ok = run(instance, device), valid = false;
    WGPUPopErrorScopeCallbackInfo scope = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    scope.mode = WGPUCallbackMode_WaitAnyOnly; scope.userdata1 = &valid;
    scope.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* user, void*) {
        *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success && type == WGPUErrorType_NoError;
        if (type != WGPUErrorType_NoError) std::fprintf(stderr, "GPU validation: %.*s\n", int(message.length), message.data);
    };
    wait(instance, wgpuDevicePopErrorScope(device, scope));
    wgpuDeviceRelease(device); wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance);
    return ok && valid ? 0 : 1;
}
