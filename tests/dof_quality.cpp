#include "render/dof_quality.hpp"

#include <cmath>
#include <limits>

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using midnafx::render::dof_quality::advance_focus;
    CHECK(advance_focus(0.0f, 1200.0f, 0.016f, 0.2f) == 1200.0f);
    CHECK(advance_focus(1200.0f, 2400.0f, 0.016f, 0.0f) == 2400.0f);
    const float advanced = advance_focus(1200.0f, 2400.0f, 0.1f, 0.2f);
    CHECK(advanced > 1200.0f && advanced < 2400.0f);
    CHECK(advance_focus(1200.0f, 2400.0f, -1.0f, 0.2f) == 1200.0f);
    CHECK(advance_focus(1200.0f, std::numeric_limits<float>::quiet_NaN(), 0.1f, 0.2f) ==
          1200.0f);
    CHECK(advance_focus(1200.0f, -1.0f, 0.1f, 0.2f) == 1200.0f);
    const float capped = advance_focus(1200.0f, 2400.0f, 10.0f, 0.2f);
    const float quarter = advance_focus(1200.0f, 2400.0f, 0.25f, 0.2f);
    CHECK(std::abs(capped - quarter) < 0.001f);
}
