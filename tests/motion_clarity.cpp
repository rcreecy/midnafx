#include "render/motion_clarity_state.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "Motion clarity check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (false)

int main() {
    using namespace midnafx::motion_clarity;

    CHECK(!eligible(GameplayContext{}));
    const GameplayContext gameplay{true, true, false, false, false, false};
    CHECK(eligible(gameplay));
    for (unsigned flags = 0; flags < 64; ++flags) {
        const GameplayContext context{(flags & 1u) != 0, (flags & 2u) != 0,
            (flags & 4u) != 0, (flags & 8u) != 0, (flags & 16u) != 0, (flags & 32u) != 0};
        CHECK(eligible(context) == (flags == 3));
    }
    auto bypass = gameplay;
    bypass.enabled = false;
    CHECK(!eligible(bypass));
    bypass = gameplay; bypass.player_present = false;
    CHECK(!eligible(bypass));
    bypass = gameplay; bypass.paused = true;
    CHECK(!eligible(bypass));
    bypass = gameplay; bypass.event_running = true;
    CHECK(!eligible(bypass));
    bypass = gameplay; bypass.demo_camera = true;
    CHECK(!eligible(bypass));
    bypass = gameplay; bypass.fading = true;
    CHECK(!eligible(bypass));

    CHECK(retained_rate(1, 49) == 0);
    CHECK(retained_rate(1, 50) == 1);
    CHECK(retained_rate(3, 50) == 2);
    CHECK(retained_rate(255, 50) == 128);
    CHECK(retained_rate(255, 30) == 77);
    CHECK(retained_rate(255, 99) == 252);
    CHECK(retained_rate(255, 1) == 3);

    for (unsigned rate = 0; rate <= 255; ++rate) {
        const auto native = static_cast<std::uint8_t>(rate);
        CHECK(retained_rate(native, 0) == 0);
        CHECK(retained_rate(native, 100) == native);
        CHECK(retained_rate(native, 101) == native);
        CHECK(retained_rate(native, std::numeric_limits<unsigned>::max()) == native);
        unsigned previous = 0;
        for (unsigned percent = 0; percent <= 100; ++percent) {
            const unsigned retained = retained_rate(native, percent);
            CHECK(retained <= rate);
            CHECK(retained >= previous);
            const int error = int(retained * 100) - int(rate * percent);
            // Nearest integer, with exact half ties rounded upward.
            CHECK(error > -50 && error <= 50);
            previous = retained;

            RateOverride override;
            std::uint8_t value = native;
            CHECK(!override.active);
            CHECK(override.begin(value, percent) == (retained < rate));
            CHECK(value == retained);
            CHECK(override.active == (retained < rate));
            if (override.active) {
                CHECK(override.original == native);
                CHECK(override.applied == retained);
            }
            CHECK(override.restore(value) == (retained < rate));
            CHECK(value == native && !override.active);
            CHECK(!override.restore(value));
            CHECK(value == native);
        }
    }
    for (unsigned percent = 0; percent <= 100; ++percent) {
        unsigned previous = 0;
        CHECK(retained_rate(0, percent) == 0);
        for (unsigned rate = 0; rate <= 255; ++rate) {
            const unsigned retained = retained_rate(static_cast<std::uint8_t>(rate), percent);
            CHECK(retained >= previous);
            previous = retained;
        }
    }

    {
        RateOverride override;
        std::uint8_t value = 200;
        CHECK(override.begin(value, 50));
        CHECK(value == 100 && override.original == 200 && override.applied == 100);
        // A repeated begin must restore the owned write before calculating the
        // next override. Otherwise reductions would compound across frames.
        CHECK(override.begin(value, 25));
        CHECK(value == 50 && override.original == 200 && override.applied == 50);
        CHECK(override.begin(value, 50));
        CHECK(value == 100 && override.original == 200 && override.applied == 100);
        CHECK(override.restore(value));
        CHECK(value == 200 && !override.active);

        value = 80;
        CHECK(override.begin(value, 50));
        CHECK(value == 40 && override.original == 80 && override.applied == 40);
        CHECK(override.restore(value));
        CHECK(value == 80);
    }
    {
        RateOverride override;
        std::uint8_t value = 200;
        CHECK(override.begin(value, 50));
        value = 77; // A native system or another mod owns this newer write.
        CHECK(!override.restore(value));
        CHECK(value == 77 && !override.active);
        CHECK(!override.restore(value));
        CHECK(value == 77);
        CHECK(override.begin(value, 50));
        CHECK(value == 39 && override.original == 77 && override.applied == 39);
        CHECK(override.restore(value));
        CHECK(value == 77);
    }
    {
        RateOverride override;
        std::uint8_t value = 200;
        CHECK(override.begin(value, 50));
        value = 120;
        // Re-entry after an intervening writer captures that writer's fresh
        // value, rather than restoring the older rate over it.
        CHECK(override.begin(value, 25));
        CHECK(value == 30 && override.original == 120 && override.applied == 30);
        CHECK(override.restore(value));
        CHECK(value == 120);
    }
    {
        RateOverride override;
        std::uint8_t value = 255;
        CHECK(override.begin(value, 0));
        CHECK(value == 0 && override.active && override.original == 255 && override.applied == 0);
        CHECK(override.restore(value));
        CHECK(value == 255 && !override.active);
        CHECK(!override.restore(value));
        CHECK(override.begin(value, 0));
        value = 1;
        CHECK(!override.restore(value));
        CHECK(value == 1 && !override.active);

        CHECK(override.begin(value, 49));
        CHECK(value == 0 && override.active);
        CHECK(override.restore(value));
        CHECK(value == 1);
    }
    {
        RateOverride override;
        std::uint8_t value = 200;
        CHECK(override.begin(value, 50));
        CHECK(!override.begin(value, 100));
        CHECK(value == 200 && !override.active);
        CHECK(!override.restore(value));
        CHECK(override.begin(value, 0));
        CHECK(!override.begin(value, std::numeric_limits<unsigned>::max()));
        CHECK(value == 200 && !override.active);

        value = 0;
        CHECK(!override.begin(value, 0));
        CHECK(value == 0 && !override.active);
        value = 1;
        CHECK(!override.restore(value));
        CHECK(value == 1);
        CHECK(!override.begin(value, 50)); // Rounding retains one, so no ownership.
        CHECK(value == 1 && !override.active);
        value = 2;
        CHECK(!override.restore(value));
        CHECK(value == 2);
    }
    std::puts("Motion clarity gameplay gates, rate bounds, rounding, monotonicity, and write ownership: PASS");
}
