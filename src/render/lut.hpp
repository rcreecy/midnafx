#pragma once
#include <webgpu/webgpu.h>
#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

namespace midnafx::lut {
inline constexpr unsigned Size = 33;
struct alignas(16) Controls {
    float intensity = 0;
    float twilight = 0;
    float padding[2]{};
};
static_assert(sizeof(Controls) == 16);

// Nonnegative finite [0,1] values only. Round to nearest, ties to even.
inline std::uint16_t half(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const int exponent = int((bits >> 23) & 255) - 127;
    if (exponent < -25) return 0;
    const auto mantissa = (bits & 0x7fffff) | 0x800000;
    const unsigned shift = exponent < -14 ? unsigned(-exponent - 1) : 13;
    const auto rounded = (mantissa + ((1u << (shift - 1)) - 1) + ((mantissa >> shift) & 1)) >> shift;
    return std::uint16_t(exponent < -14 ? rounded : ((exponent + 14) << 10) + rounded);
}

inline std::vector<std::uint16_t> lattice(unsigned look = 0) {
    std::vector<std::uint16_t> data(Size * Size * Size * 4);
    for (unsigned b = 0; b < Size; ++b)
        for (unsigned g = 0; g < Size; ++g)
            for (unsigned r = 0; r < Size; ++r) {
                const auto i = ((b * Size + g) * Size + r) * 4;
                std::array<float, 3> color{float(r) / (Size - 1), float(g) / (Size - 1), float(b) / (Size - 1)};
                // Restrained candidates in scene-code space. Endpoints stay fixed.
                if (look == 1 || look == 2) {
                    for (auto& channel : color)
                        channel += 0.04f * channel * (1 - channel) * (2 * channel - 1);
                    if (look == 2) {
                        color[0] -= 0.012f * color[0] * (1 - color[0]);
                        color[1] += 0.008f * color[1] * (1 - color[1]);
                        color[2] += 0.030f * color[2] * (1 - color[2]);
                    }
                }
                for (unsigned c = 0; c < 3; ++c) data[i + c] = half(std::clamp(color[c], 0.0f, 1.0f));
                data[i + 3] = half(1.0f);
            }
    return data;
}
inline std::vector<std::uint16_t> identity() { return lattice(); }

struct Texture {
    WGPUTexture texture = nullptr;
    WGPUTextureView view = nullptr;
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture() {
        if (view) wgpuTextureViewRelease(view);
        if (texture) wgpuTextureRelease(texture);
    }
    bool create(WGPUDevice device, const std::vector<std::uint16_t>& data) {
        if (texture || data.size() != Size * Size * Size * 4) return false;
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX color lattice", WGPU_STRLEN};
        desc.dimension = WGPUTextureDimension_3D;
        desc.size = {Size, Size, Size};
        desc.format = WGPUTextureFormat_RGBA16Float;
        desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        texture = wgpuDeviceCreateTexture(device, &desc);
        if (!texture) return false;
        view = wgpuTextureCreateView(texture, nullptr);
        if (!view) return false;
        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        destination.texture = texture;
        WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
        layout.bytesPerRow = Size * 8;
        layout.rowsPerImage = Size;
        auto queue = wgpuDeviceGetQueue(device);
        wgpuQueueWriteTexture(queue, &destination, data.data(), data.size() * sizeof(data[0]), &layout, &desc.size);
        wgpuQueueRelease(queue);
        return true;
    }
};

inline WGPUSampler sampler(WGPUDevice device) {
    WGPUSamplerDescriptor desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    desc.addressModeU = desc.addressModeV = desc.addressModeW = WGPUAddressMode_ClampToEdge;
    desc.magFilter = desc.minFilter = WGPUFilterMode_Linear;
    return wgpuDeviceCreateSampler(device, &desc);
}
} // namespace midnafx::lut
