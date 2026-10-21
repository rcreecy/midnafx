// Compile against the same pinned Aurora headers and flags as the host.
// This executable only reports C++ object sizes; it initializes no GPU.
#include "gfx/frame_packet.hpp"
#include "gx/pipeline.hpp"
#include <cstdio>

int main() {
    using namespace aurora;
    std::printf("{\"inline_payload\":%zu,\"command\":%zu,\"draw_command\":%zu,"
                "\"custom_draw_command\":%zu,\"encoder_task\":%zu,\"texture_ref\":%zu,"
                "\"gx_bind_groups\":%zu,\"gx_draw_data\":%zu}\n",
                gfx::InlineDrawPayloadSize, sizeof(gfx::detail::Command), sizeof(gfx::detail::DrawCommand),
                sizeof(gfx::detail::CustomDrawCommand), sizeof(gfx::detail::EncoderTask),
                sizeof(gfx::TextureRef), sizeof(gx::GXBindGroups), sizeof(gx::DrawData));
}
