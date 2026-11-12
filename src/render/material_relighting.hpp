#pragma once

namespace midnafx::render::material_relighting {
bool available() noexcept;
bool surface_relief_available() noexcept;
bool surface_profiles_available() noexcept;
bool set_surface_profile(unsigned profile) noexcept;
void update();
void shutdown();
}
