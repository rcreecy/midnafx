#pragma once
#include <webgpu/webgpu.h>
#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
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
                // Scene-code palette with warm light, cool shade, and richer foliage.
                if (look == 1 || look == 2) {
                    for (auto& channel : color)
                        channel += 0.04f * channel * (1 - channel) * (2 * channel - 1);
                    const float luma = 0.2126f * color[0] + 0.7152f * color[1] + 0.0722f * color[2];
                    const float shadow = (1 - luma) * (1 - luma);
                    const float light = luma * luma;
                    const float foliage = std::clamp((color[1] - 0.88f * color[0]) * 8.0f, 0.0f, 1.0f) *
                                          std::clamp((color[1] - color[2]) * 6.0f, 0.0f, 1.0f);
                    const std::array<float, 3> tone{0.13f * light - 0.045f * shadow - 0.045f * foliage,
                                                   0.035f * light + 0.065f * foliage,
                                                   0.11f * shadow - 0.10f * light};
                    for (unsigned c = 0; c < 3; ++c)
                        color[c] += (color[c] - luma) * 0.075f + tone[c] * color[c] * (1 - color[c]);
                    if (look == 1) {
                        // Build color richness in the midtones. Protect near-neutral
                        // highlights, deep shadows and already saturated colors.
                        const auto smooth = [](float a, float b, float x) {
                            const float t = std::clamp((x-a)/(b-a), 0.0f, 1.0f);
                            return t*t*(3-2*t);
                        };
                        const float mids = smooth(0.035f, 0.22f, luma) * (1-smooth(0.68f, 0.96f, luma));
                        const float maximum = std::max({color[0], color[1], color[2], 0.001f});
                        const float chroma = maximum - std::min({color[0], color[1], color[2]});
                        const float muted = 1-std::clamp(chroma/maximum, 0.0f, 1.0f);
                        const float warm = smooth(0.0f, 0.12f, color[0]-color[1]) *
                                           smooth(0.0f, 0.10f, color[1]-color[2]);
                        const float vibrance = mids * (0.18f + 0.14f*muted) * (1-0.55f*warm);
                        const float leaf = smooth(-0.02f, 0.13f, color[1]-0.88f*color[0]) *
                                           smooth(0.02f, 0.18f, color[1]-color[2]) * mids;
                        const std::array<float, 3> leaf_tone{-0.020f, 0.020f, -0.012f};
                        const float base = 0.2126f*color[0] + 0.7152f*color[1] + 0.0722f*color[2];
                        std::array<float, 3> rich{};
                        for (unsigned c = 0; c < 3; ++c)
                            rich[c] = color[c] + (color[c]-base)*vibrance + leaf_tone[c]*leaf;
                        const float target = std::clamp(base + 0.12f*base*(1-base)*(2*base-1), 0.0f, 1.0f);
                        const float rich_luma = 0.2126f*rich[0] + 0.7152f*rich[1] + 0.0722f*rich[2];
                        // Compress toward luminance instead of clipping individual
                        // channels, keeping saturated colors' hue transitions smooth.
                        float gamut = 1;
                        for (const float channel : rich) {
                            const float delta = channel-rich_luma;
                            if (delta > 0) gamut = std::min(gamut, (1-target)/delta);
                            if (delta < 0) gamut = std::min(gamut, -target/delta);
                        }
                        for (unsigned c = 0; c < 3; ++c) color[c] = target + gamut*(rich[c]-rich_luma);
                    }
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
