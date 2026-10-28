#include "game/camera_fov.hpp"

#include <cmath>
#include <initializer_list>
#include <limits>

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx::camera_fov;
    CHECK(std::abs(scale_vertical(60.0f, 1.0f) - 60.0f) < 0.0001f);
    CHECK(scale_vertical(60.0f, 1.15f) > 60.0f);
    CHECK(scale_vertical(60.0f, 0.85f) < 60.0f);
    CHECK(scale_vertical(60.0f, 0.0f) == 60.0f);
    CHECK(std::isnan(scale_vertical(std::numeric_limits<float>::quiet_NaN(), 1.1f)));
    CHECK(std::abs(advance(0.0f, 1.0f, 3, 1.0f) - 0.1f) < 0.0001f);
    CHECK(advance(0.95f, 1.0f, 4, 1.0f) == 1.0f);
    CHECK(std::abs(advance(0.0f, 1.0f, std::numeric_limits<std::uint64_t>::max(), 1.0f) -
                   (4.0f / 30.0f)) < 0.0001f);
    CHECK(advance(0.2f, 0.0f, 4, 0.0f) == 0.0f);

    CHECK(movement_target(0, 10) == 0);
    CHECK(movement_target(3.5f, 10) == 0);
    CHECK(movement_target(5, 10) > 0 && movement_target(5, 10) < 1);
    CHECK(movement_target(9, 10) == 1);
    CHECK(movement_target(100, 10) == 1);
    CHECK(movement_target(-9, 10) == movement_target(9, 10));
    float previous_target = 0;
    for (unsigned speed = 0; speed <= 12; ++speed) {
        const float target = movement_target(float(speed), 10);
        CHECK(std::isfinite(target) && target >= previous_target && target >= 0 && target <= 1);
        CHECK(target == movement_target(-float(speed), 10));
        CHECK(std::abs(target - movement_target(float(speed) * 3, 30)) < 0.000001f);
        previous_target = target;
    }
    const float malformed[]{std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    for (float bad : malformed) {
        CHECK(movement_target(bad, 10) == 0);
        CHECK(movement_target(9, bad) == 0);
    }
    CHECK(movement_target(9, 0) == 0);
    CHECK(movement_target(9, -10) == 0);

    const auto reset_state = [](const MovementFraming& framing) {
        return framing.amount == 0 && framing.ticks == 0 && framing.frame == 0 && !framing.sampled;
    };
    MovementFraming first;
    CHECK(reset_state(first));
    const float first_step = first.update(9, 10, true, 0, 0, 1);
    CHECK(first.sampled && first_step > 0 && first_step < 1);
    MovementFraming late, backwards;
    CHECK(late.update(9, 10, true, 100000, 200000, 1) == first_step);
    CHECK(backwards.update(-9, 10, true, 100000, 200000, 1) == first_step);
    late.reset();
    CHECK(reset_state(late));

    MovementFraming trajectory;
    float previous = 0;
    for (std::uint64_t tick = 1; tick <= 30; ++tick) {
        const float amount = trajectory.update(9, 10, true, tick, tick, 1);
        CHECK(amount >= previous && amount >= 0 && amount <= 1);
        previous = amount;
    }
    CHECK(std::abs(trajectory.amount - 1) < 0.00001f);
    const float first_decay = trajectory.update(0, 10, true, 31, 31, 1);
    CHECK(first_decay < previous && first_decay > 0);
    CHECK(std::abs(first_step - 2 * (previous - first_decay)) < 0.00001f);
    previous = first_decay;
    for (std::uint64_t tick = 32; tick <= 90; ++tick) {
        const float amount = trajectory.update(0, 10, true, tick, tick, 1);
        CHECK(amount <= previous && amount >= 0 && amount <= 1);
        previous = amount;
        if (tick == 60) CHECK(amount > 0.4f && amount < 0.6f);
    }
    CHECK(trajectory.amount < 0.00001f);
    CHECK(trajectory.update(9, 10, true, 91, 91, 0) == 1);
    CHECK(trajectory.update(0, 10, true, 92, 92, 0) == 0);

    MovementFraming repeated;
    CHECK(repeated.update(9, 10, true, 100, 200, 1) == first_step);
    CHECK(repeated.update(0, 10, true, 100, 201, 1) == first_step);
    CHECK(repeated.update(0, 10, true, 101, 200, 1) == first_step);
    CHECK(repeated.update(9, 10, true, 101, 201, 1) > first_step);
    CHECK(repeated.update(9, 10, false, 101, 201, 1) == 0);
    CHECK(reset_state(repeated));
    CHECK(repeated.update(9, 10, true, 101, 201, 1) == first_step);
    for (float bad : malformed) {
        CHECK(repeated.update(9, 10, true, 102, 202, 0) == 1);
        CHECK(repeated.update(bad, 10, true, 102, 202, 1) == 0);
        CHECK(reset_state(repeated));
        CHECK(repeated.update(9, 10, true, 102, 202, 0) == 1);
        CHECK(repeated.update(9, bad, true, 102, 202, 1) == 0);
        CHECK(reset_state(repeated));
        CHECK(repeated.update(9, 10, true, 102, 202, 0) == 1);
        CHECK(repeated.update(9, 10, true, 102, 202, bad) == 0);
        CHECK(reset_state(repeated));
    }
    for (float invalid_max : {0.0f, -1.0f}) {
        CHECK(repeated.update(9, 10, true, 102, 202, 0) == 1);
        CHECK(repeated.update(9, invalid_max, true, 102, 202, 1) == 0);
        CHECK(reset_state(repeated));
    }
    CHECK(repeated.update(9, 10, true, 102, 202, 0) == 1);
    CHECK(repeated.update(9, 10, true, 102, 202, -1) == 0);
    CHECK(reset_state(repeated));

    // Re-entry and discontinuities consume one fresh tick, never accumulated
    // wall time or unsigned clock wraparound.
    for (unsigned discontinuity = 0; discontinuity < 4; ++discontinuity) {
        MovementFraming framing;
        CHECK(framing.update(9, 10, true, 100, 200, 0) == 1);
        const std::uint64_t next_tick = discontinuity == 0 ? 99 : discontinuity == 2 ? 105 :
                                        discontinuity == 3 ? 100000 : 101;
        const std::uint64_t next_frame = discontinuity == 1 ? 199 : 201;
        CHECK(framing.update(9, 10, true, next_tick, next_frame, 1) == first_step);
        CHECK(framing.sampled);
    }
    MovementFraming wrapping;
    CHECK(wrapping.update(9, 10, true, std::numeric_limits<std::uint64_t>::max(), 100, 0) == 1);
    CHECK(wrapping.update(9, 10, true, 0, 101, 1) == first_step);
    MovementFraming boundary, sequential;
    boundary.update(9, 10, true, 100, 200, 1);
    sequential.update(9, 10, true, 100, 200, 1);
    for (std::uint64_t tick = 101; tick <= 104; ++tick) sequential.update(9, 10, true, tick, tick + 100, 1);
    CHECK(std::abs(boundary.update(9, 10, true, 104, 204, 1) - sequential.amount) < 0.000001f);

    MovementFraming dense, sparse;
    dense.update(0, 10, true, 0, 0, 1);
    sparse.update(0, 10, true, 0, 0, 1);
    for (std::uint64_t tick = 1; tick <= 90; ++tick) {
        const float speed = tick <= 30 || tick > 60 ? 9.0f : 0.0f;
        const float amount = dense.update(speed, 10, true, tick, tick * 2, 1);
        CHECK(dense.update(speed, 10, true, tick, tick * 2 + 1, 1) == amount);
        if (tick % 3 == 0)
            CHECK(std::abs(sparse.update(speed, 10, true, tick, tick * 2, 1) - amount) < 0.00001f);
    }
    for (float native : {35.0f, 60.0f, 90.0f, 120.0f})
        for (float user_scale : {0.85f, 1.0f, 1.35f})
            for (float speed : {0.0f, 5.0f, 9.0f, -9.0f, 100.0f}) {
                const float amount = movement_target(speed, 10);
                const float base = scale_vertical(native, user_scale);
                const float boosted = scale_vertical(native, user_scale * (1 + 0.08f * amount));
                CHECK(boosted >= base && boosted <= scale_vertical(native, user_scale * 1.08f));
                if (speed == 0) CHECK(boosted == base);
            }
}
