// Host tests for the firmware's motion governor (firmware/axiscontroller/app/
// axis_motion.c). The governor is what stands between motion requests and a
// heavy arm, so these check its guarantees directly, including under random
// streams of requests: velocity, acceleration and jerk stay within the
// axis's limits, and targets and soft limits are never overshot.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

extern "C" {
#include "axis_motion.h"
}

namespace {

constexpr float k_dt = 0.001f;

// Arm-like limits: modest acceleration, gentle jerk.
AxisMotionLimits arm_limits()
{
    return AxisMotionLimits{
        .max_velocity = 3200.0f,
        .max_accel = 6400.0f,
        .max_decel = 6400.0f,
        .max_jerk = 64000.0f,
        .quick_stop_decel = 12800.0f,
    };
}

// Steps the governor and checks every update against the limits that applied
// while it ran. Limits are taken from the governor itself, so tests that
// change them mid-run are checked against the new ones.
class Checker {
public:
    explicit Checker(AxisMotion& motion) : motion_(motion) {}

    void run(float seconds)
    {
        const int updates = static_cast<int>(std::lround(seconds / k_dt));
        for (int i = 0; i < updates; ++i) {
            step();
        }
    }

    // Runs until idle or the time budget is spent; returns whether it settled.
    bool settle(float max_seconds)
    {
        const int updates = static_cast<int>(std::lround(max_seconds / k_dt));
        for (int i = 0; i < updates && !axis_motion_is_idle(&motion_); ++i) {
            step();
        }
        return axis_motion_is_idle(&motion_);
    }

    void step()
    {
        const float velocity = motion_.velocity;
        const float accel = motion_.accel;
        const AxisMotionLimits before = motion_.limits;
        axis_motion_update(&motion_, k_dt);
        const AxisMotionLimits& after = motion_.limits;

        // Limits only change when the axis comes to rest, so each update must
        // respect those in force either side of it.
        const float tolerance = 1.001f;
        const float jerk_step = std::max(before.max_jerk, after.max_jerk) * k_dt;
        const float hardest = std::max({before.max_accel, before.quick_stop_decel, after.max_accel, after.quick_stop_decel});
        const float fastest = std::max(before.max_velocity, after.max_velocity);

        EXPECT_LE(std::fabs(motion_.accel - accel), jerk_step * tolerance + 1e-3f) << where("jerk");
        EXPECT_LE(std::fabs(motion_.accel), hardest * tolerance) << where("acceleration");
        // Velocity can only change by the acceleration applied over the update.
        EXPECT_LE(std::fabs(motion_.velocity - velocity), hardest * k_dt * tolerance + 1e-3f) << where("velocity step");
        EXPECT_LE(std::fabs(motion_.velocity), fastest * tolerance + 0.5f) << where("velocity limit");
        if (motion_.soft_limits_enabled && soft_checked_) {
            worst_soft_crossing_ = std::max({worst_soft_crossing_, motion_.soft_min - motion_.position,
                                             motion_.position - motion_.soft_max});
            EXPECT_GE(motion_.position, motion_.soft_min - soft_tolerance_) << where("soft min");
            EXPECT_LE(motion_.position, motion_.soft_max + soft_tolerance_) << where("soft max");
        }
        ++updates_;
    }

    void check_soft_limits(bool enabled) { soft_checked_ = enabled; }
    void soft_tolerance(double steps) { soft_tolerance_ = steps; }
    double worst_soft_crossing() const { return worst_soft_crossing_; }

private:
    std::string where(const char* what) const
    {
        return std::string(what) + " at update " + std::to_string(updates_) +
               " pos=" + std::to_string(motion_.position) + " vel=" + std::to_string(motion_.velocity) +
               " acc=" + std::to_string(motion_.accel) + " mode=" + std::to_string(motion_.mode) +
               " cruise=" + std::to_string(motion_.cruise_velocity) + " target_v=" +
               std::to_string(motion_.target_velocity) + " jerk=" + std::to_string(motion_.limits.max_jerk) +
               " max_a=" + std::to_string(motion_.limits.max_accel);
    }

    AxisMotion& motion_;
    bool soft_checked_{true};
    double soft_tolerance_{0.5};
    double worst_soft_crossing_{0.0};
    long updates_{0};
};

double overshoot_past(double target, double start, double position)
{
    return (position - target) * (target > start ? 1.0 : -1.0);
}

} // namespace

TEST(AxisMotionTests, MovesToTargetWithinLimitsAndSettlesExactly)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    axis_motion_move_to(&motion, 20000.0, 3200.0f, 6400.0f, 6400.0f);
    double worst_overshoot = 0.0;
    for (int i = 0; i < 20000 && !axis_motion_is_idle(&motion); ++i) {
        checker.step();
        worst_overshoot = std::max(worst_overshoot, overshoot_past(20000.0, 0.0, motion.position));
    }

    ASSERT_TRUE(axis_motion_is_idle(&motion));
    EXPECT_DOUBLE_EQ(motion.position, 20000.0);
    EXPECT_LE(worst_overshoot, 0.5);
}

TEST(AxisMotionTests, ReversalRampsThroughZeroInsteadOfStopping)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    // Jog forward to full speed, then flip to jog backward: the case that
    // used to stop dead.
    axis_motion_run(&motion, 3200.0f, 6400.0f, 6400.0f);
    checker.run(1.0f);
    ASSERT_NEAR(motion.velocity, 3200.0f, 1.0f);

    axis_motion_run(&motion, -3200.0f, 6400.0f, 6400.0f);
    float previous = motion.velocity;
    float slowest_change = 0.0f;
    bool crossed_zero = false;
    for (int i = 0; i < 2000; ++i) {
        checker.step();
        slowest_change = std::max(slowest_change, std::fabs(motion.velocity - previous));
        crossed_zero = crossed_zero || (previous > 0.0f && motion.velocity <= 0.0f);
        previous = motion.velocity;
    }

    EXPECT_TRUE(crossed_zero);
    EXPECT_NEAR(motion.velocity, -3200.0f, 1.0f);
    // Never more than one update's worth of deceleration.
    EXPECT_LE(slowest_change, 6400.0f * k_dt * 1.001f);
}

TEST(AxisMotionTests, RetargetBehindAMovingAxisBrakesThenReturns)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    axis_motion_move_to(&motion, 50000.0, 3200.0f, 6400.0f, 6400.0f);
    checker.run(1.0f);
    const double turn_point = motion.position;

    axis_motion_move_to(&motion, 0.0, 3200.0f, 6400.0f, 6400.0f);
    ASSERT_TRUE(checker.settle(30.0f));
    EXPECT_DOUBLE_EQ(motion.position, 0.0);
    EXPECT_GT(turn_point, 0.0);
}

TEST(AxisMotionTests, QuickStopIsShorterThanStopAndBothStayWithinLimits)
{
    const auto limits = arm_limits();
    auto stop_distance = [&](float decel) {
        AxisMotion motion;
        axis_motion_init(&motion, &limits, 0.0);
        Checker checker(motion);
        axis_motion_run(&motion, 3200.0f, 6400.0f, 6400.0f);
        checker.run(1.0f);
        const double from = motion.position;
        axis_motion_stop(&motion, decel);
        EXPECT_TRUE(checker.settle(5.0f));
        return motion.position - from;
    };

    const double normal = stop_distance(6400.0f);
    const double quick = stop_distance(limits.quick_stop_decel);
    const double over_limit = stop_distance(1e9f);

    EXPECT_LT(quick, normal);
    // Asking for more than quick_stop_decel gets quick_stop_decel, no more.
    EXPECT_NEAR(over_limit, quick, 1.0);
    // The plan's v^2/2d, plus the half window the output trails it by. The
    // window spans 2 * quick_stop_decel / jerk.
    const double window = 2.0 * limits.quick_stop_decel / limits.max_jerk;
    EXPECT_NEAR(normal, 3200.0 * 3200.0 / (2 * 6400.0) + 3200.0 * window / 2, 5.0);
}

TEST(AxisMotionTests, RequestsAboveTheLimitsAreClamped)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    axis_motion_move_to(&motion, 1e6, 1e9f, 1e9f, 1e9f);
    checker.run(2.0f);
    EXPECT_NEAR(motion.velocity, limits.max_velocity, 0.01f);
}

TEST(AxisMotionTests, NeverCrossesSoftLimitsWhenRunningOrTargeting)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    axis_motion_set_soft_limits(&motion, true, -5000.0, 5000.0);
    Checker checker(motion);

    axis_motion_run(&motion, 3200.0f, 6400.0f, 6400.0f);
    checker.run(6.0f);
    EXPECT_NEAR(motion.position, 5000.0, 1.0);
    EXPECT_NEAR(motion.velocity, 0.0f, 1.0f);

    axis_motion_move_to(&motion, -1e7, 3200.0f, 6400.0f, 6400.0f);
    ASSERT_TRUE(checker.settle(20.0f));
    EXPECT_DOUBLE_EQ(motion.position, -5000.0);
}

TEST(AxisMotionTests, NewLimitsWaitUntilTheAxisIsAtRest)
{
    auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    axis_motion_run(&motion, 3200.0f, 6400.0f, 6400.0f);
    checker.run(1.0f);

    limits.max_velocity = 800.0f;
    axis_motion_set_limits(&motion, &limits);
    checker.run(0.5f);
    // Still moving under the limits it started with.
    EXPECT_NEAR(motion.velocity, 3200.0f, 1.0f);
    EXPECT_EQ(motion.limits.max_velocity, 3200.0f);

    axis_motion_stop(&motion, 6400.0f);
    ASSERT_TRUE(checker.settle(5.0f));
    EXPECT_EQ(motion.limits.max_velocity, 800.0f);

    axis_motion_run(&motion, 3200.0f, 6400.0f, 6400.0f);
    checker.run(1.0f);
    EXPECT_NEAR(motion.velocity, 800.0f, 2.0f);
}

TEST(AxisMotionTests, RedefiningPositionWhileMovingKeepsTheMotion)
{
    const auto limits = arm_limits();
    AxisMotion motion;
    axis_motion_init(&motion, &limits, 0.0);
    Checker checker(motion);

    axis_motion_move_to(&motion, 10000.0, 3200.0f, 6400.0f, 6400.0f);
    checker.run(0.5f);
    const double remaining = motion.target_position - motion.position;
    const float velocity = motion.velocity;

    axis_motion_redefine_position(&motion, 0.0);
    EXPECT_DOUBLE_EQ(motion.target_position - motion.position, remaining);
    EXPECT_EQ(motion.velocity, velocity);
    ASSERT_TRUE(checker.settle(10.0f));
    EXPECT_NEAR(motion.position, remaining, 1e-6);
}

TEST(AxisMotionTests, SanitisesUnusableLimits)
{
    AxisMotionLimits limits{
        .max_velocity = -5.0f,
        .max_accel = 0.0f,
        .max_decel = NAN,
        .max_jerk = 1000.0f,
        .quick_stop_decel = 0.5f,
    };
    EXPECT_FALSE(axis_motion_sanitise_limits(&limits, 15000.0f));
    EXPECT_GT(limits.max_velocity, 0.0f);
    EXPECT_GT(limits.max_accel, 0.0f);
    EXPECT_GT(limits.max_decel, 0.0f);
    EXPECT_GE(limits.quick_stop_decel, limits.max_decel);

    AxisMotionLimits fast = arm_limits();
    fast.max_velocity = 1e6f;
    axis_motion_sanitise_limits(&fast, 15000.0f);
    EXPECT_EQ(fast.max_velocity, 15000.0f);
}

// Random streams of requests across a spread of limit sets, including stiff
// bench-motor limits and soft, heavy-arm ones, with limits changed and soft
// limits toggled mid-motion. Every update is checked against the velocity,
// acceleration and jerk limits that protect the drive train, and targets and
// soft limits must hold to rounding error: the governor's construction
// guarantees them under any limits at all.
constexpr double k_any_limits_tolerance = 1e-6;

struct Worst {
    double overshoot{0.0};
    double soft_crossing{0.0};
};

// Whether a quick stop can still keep the axis within +/-`edge`. The output
// trails the plan, so this is judged on the plan: where it is, how far a
// quick stop from its speed carries it, and the positions still in the
// smoothing window. (On the limits in force, not any still pending.)
bool quick_stop_stays_within(const AxisMotion& motion, double edge)
{
    const double speed = std::fabs(motion.plan_velocity);
    if (std::fabs(motion.plan_position) + speed * speed / (2.0 * motion.limits.quick_stop_decel) >= edge) {
        return false;
    }
    for (int i = 0; i <= motion.window; ++i) {
        if (std::fabs(motion.history[i]) >= edge) {
            return false;
        }
    }
    return true;
}

Worst run_random_scenarios(unsigned seed, int scenarios)
{
    Worst worst;
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    auto random_limits = [&] {
        AxisMotionLimits limits{
            .max_velocity = 200.0f + unit(random) * 8000.0f,
            .max_accel = 200.0f + unit(random) * 40000.0f,
            .max_decel = 200.0f + unit(random) * 40000.0f,
            .max_jerk = 2000.0f + unit(random) * 400000.0f,
            .quick_stop_decel = 0.0f,
        };
        limits.quick_stop_decel = limits.max_decel * (1.0f + unit(random) * 2.0f);
        axis_motion_sanitise_limits(&limits, 15000.0f);
        return limits;
    };

    for (int scenario = 0; scenario < scenarios; ++scenario) {
        AxisMotionLimits limits = random_limits();
        AxisMotion motion;
        axis_motion_init(&motion, &limits, 0.0);
        bool soft = scenario % 3 == 0;
        if (soft) {
            axis_motion_set_soft_limits(&motion, true, -20000.0, 20000.0);
        }
        Checker checker(motion);
        checker.soft_tolerance(k_any_limits_tolerance);
        SCOPED_TRACE("seed " + std::to_string(seed) + " scenario " + std::to_string(scenario));

        // ROBERTA_MOTION_TRACE="<seed>:<scenario>" prints that scenario's
        // requests, for replaying a failure.
        const char* trace_env = std::getenv("ROBERTA_MOTION_TRACE");
        const bool trace = trace_env != nullptr &&
            std::string(trace_env) == std::to_string(seed) + ":" + std::to_string(scenario);
        if (trace) {
            std::cout << "limits v=" << limits.max_velocity << " a=" << limits.max_accel << " d=" << limits.max_decel
                      << " j=" << limits.max_jerk << " q=" << limits.quick_stop_decel << " soft=" << soft << "\n";
        }

        for (int request = 0; request < 40; ++request) {
            if (trace) {
                std::cout << "request " << request << " pos=" << motion.position << " vel=" << motion.velocity
                          << " acc=" << motion.accel << " mode=" << motion.mode << " soft=" << motion.soft_limits_enabled
                          << " limits d=" << motion.limits.max_decel << " q=" << motion.limits.quick_stop_decel
                          << " j=" << motion.limits.max_jerk << " a=" << motion.limits.max_accel
                          << " target=" << motion.target_position << " cruise=" << motion.cruise_velocity
                          << " req_a=" << motion.request_accel << " req_d=" << motion.request_decel << "\n";
            }
            const float choice = unit(random);
            const float velocity = 50.0f + unit(random) * 10000.0f;
            const float accel = 50.0f + unit(random) * 60000.0f;
            const float decel = 50.0f + unit(random) * 60000.0f;
            if (choice < 0.40f) {
                axis_motion_move_to(&motion, (unit(random) - 0.5f) * 60000.0f, velocity, accel, decel);
            } else if (choice < 0.68f) {
                axis_motion_run(&motion, (unit(random) < 0.5f ? -1.0f : 1.0f) * velocity, accel, decel);
            } else if (choice < 0.82f) {
                axis_motion_stop(&motion, decel);
            } else if (choice < 0.90f) {
                axis_motion_stop(&motion, limits.quick_stop_decel);
            } else if (choice < 0.96f) {
                // New limits mid-motion; they apply when the axis next rests.
                limits = random_limits();
                axis_motion_set_limits(&motion, &limits);
            } else if (soft || quick_stop_stays_within(motion, 20000.0)) {
                // Soft limits only switched on while a quick stop could still
                // stop the axis inside them; otherwise crossing is physics.
                soft = !soft;
                axis_motion_set_soft_limits(&motion, soft, -20000.0, 20000.0);
            }
            checker.run(unit(random) * 1.5f);
        }

        // Whatever state it was left in, it must come to rest, then land
        // exactly on a target from rest. (A target closer than the axis can
        // stop from full speed is overshot and returned to; that is physics,
        // so landing accuracy is measured from rest.)
        axis_motion_stop(&motion, limits.quick_stop_decel);
        if (!checker.settle(200.0f)) {
            ADD_FAILURE() << "did not come to rest after a quick stop";
            continue;
        }
        const double target = soft ? 12345.0 : -23456.0;
        axis_motion_move_to(&motion, target, limits.max_velocity, limits.max_accel, limits.max_decel);
        const double start = motion.position;
        double worst_overshoot = 0.0;
        // Generous time for the distance at the slowest random cruise.
        const int budget = static_cast<int>(std::fabs(start - target) / limits.max_velocity / k_dt) + 100000;
        for (int i = 0; i < budget && !axis_motion_is_idle(&motion); ++i) {
            checker.step();
            if (std::fabs(start - target) > 1.0) {
                worst_overshoot = std::max(worst_overshoot, overshoot_past(target, start, motion.position));
            }
        }
        if (!axis_motion_is_idle(&motion)) {
            ADD_FAILURE() << "did not settle on target: pos=" << motion.position << " vel=" << motion.velocity << " acc=" << motion.accel
            << " mode=" << motion.mode << " target=" << motion.target_position
            << " cruise=" << motion.cruise_velocity << " accel=" << motion.request_accel
            << " decel=" << motion.request_decel << " jerk=" << limits.max_jerk;
            continue;
        }
        EXPECT_DOUBLE_EQ(motion.position, target);
        worst.overshoot = std::max(worst.overshoot, worst_overshoot);
        worst.soft_crossing = std::max(worst.soft_crossing, checker.worst_soft_crossing());
        EXPECT_LE(worst_overshoot, k_any_limits_tolerance)
            << "start=" << start << " target=" << target << " limits v=" << limits.max_velocity
            << " a=" << limits.max_accel << " d=" << limits.max_decel << " j=" << limits.max_jerk
            << " q=" << limits.quick_stop_decel << " soft=" << motion.soft_limits_enabled;
    }
    return worst;
}

TEST(AxisMotionTests, RandomRequestStreamsNeverBreakTheLimits)
{
    // ROBERTA_MOTION_FUZZ_SCENARIOS raises the count for a long soak.
    const char* count_env = std::getenv("ROBERTA_MOTION_FUZZ_SCENARIOS");
    const int scenarios = count_env != nullptr ? std::max(1, std::atoi(count_env)) : 300;

    Worst worst;
    for (const unsigned seed : {1234u, 98765u}) {
        const Worst run = run_random_scenarios(seed, scenarios);
        worst.overshoot = std::max(worst.overshoot, run.overshoot);
        worst.soft_crossing = std::max(worst.soft_crossing, run.soft_crossing);
    }
    RecordProperty("worst_target_overshoot_steps", std::to_string(worst.overshoot));
    RecordProperty("worst_soft_limit_crossing_steps", std::to_string(worst.soft_crossing));
    std::cout << "worst target overshoot " << worst.overshoot << " steps, worst soft-limit crossing "
              << worst.soft_crossing << " steps\n";
}
