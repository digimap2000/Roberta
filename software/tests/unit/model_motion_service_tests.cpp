#include <gtest/gtest.h>

#include <roberta/model/robot_model.hpp>
#include <roberta/motion/motion.hpp>
#include <roberta/service/robot_service.hpp>
#include <roberta/service/single_axis_service.hpp>
#include <roberta/sim/simulated_axis_device.hpp>

namespace {

TEST(ModelTests, LoadsUrdfAndComputesForwardKinematics)
{
    const auto path = std::filesystem::path(ROBERTA_URDF_PATH);
    auto config = roberta::model::load_robot_from_urdf(path);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->joints.size(), 6u);

    roberta::model::RobotModel model(*config);
    auto fk = model.forward_kinematics(std::vector<double>(6, 0.0));
    ASSERT_TRUE(fk.has_value());
    EXPECT_NEAR(fk->tool_pose.translation.z, 1.485, 0.05);
}

TEST(MotionTests, PlannerAndCoordinatorExecuteSynchronizedMoveInSimulation)
{
    const auto path = std::filesystem::path(ROBERTA_URDF_PATH);
    auto config = roberta::model::load_robot_from_urdf(path);
    ASSERT_TRUE(config.has_value());
    roberta::model::RobotModel model(*config);

    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes;
    std::vector<roberta::model::AxisCalibration> calibrations;
    for (std::uint32_t axis_id = 1; axis_id <= 6; ++axis_id) {
        auto axis = std::make_shared<roberta::sim::SimulatedAxisDevice>(axis_id);
        ASSERT_TRUE(axis->connect().has_value());
        ASSERT_TRUE(axis->set_outputs_enabled(true).has_value());
        ASSERT_TRUE(axis->set_motor_enabled(true).has_value());
        ASSERT_TRUE(axis->set_motion_profile({.velocity_steps_per_second = 5000, .accel_steps_per_second_squared = 5000, .home_velocity_steps_per_second = 100}).has_value());
        axes.push_back(axis);
        calibrations.push_back(roberta::model::AxisCalibration{axis_id, 10.0});
    }

    roberta::motion::MotionPlanner planner(model);
    auto plan = planner.plan_joint_move(std::vector<double>(6, 0.0), std::vector<double>{0.1, -0.2, 0.15, 0.0, 0.05, -0.05}, std::chrono::microseconds(1000));
    ASSERT_TRUE(plan.has_value());

    roberta::motion::MotionCoordinator coordinator(axes, calibrations);
    ASSERT_TRUE(coordinator.dispatch(*plan).has_value());
    ASSERT_TRUE(coordinator.advance(std::chrono::milliseconds(20)).has_value());
    ASSERT_TRUE(coordinator.advance(std::chrono::milliseconds(200)).has_value());
    auto snapshots = coordinator.snapshots();
    ASSERT_TRUE(snapshots.has_value());
    EXPECT_EQ(snapshots->size(), 6u);
    EXPECT_NE((*snapshots)[0].current_position_steps, 0);
}

TEST(ServiceTests, ServiceSubmitsAndCompletesJointAndCartesianMoves)
{
    const auto path = std::filesystem::path(ROBERTA_URDF_PATH);
    auto config = roberta::model::load_robot_from_urdf(path);
    ASSERT_TRUE(config.has_value());
    roberta::model::RobotModel model(*config);

    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes;
    std::vector<roberta::model::AxisCalibration> calibrations;
    for (std::uint32_t axis_id = 1; axis_id <= 6; ++axis_id) {
        auto axis = std::make_shared<roberta::sim::SimulatedAxisDevice>(axis_id);
        axes.push_back(axis);
        calibrations.push_back(roberta::model::AxisCalibration{axis_id, 25.0});
    }

    roberta::service::RobotService service(std::move(model), axes, calibrations);
    ASSERT_TRUE(service.connect_all().has_value());
    ASSERT_TRUE(service.enable_all().has_value());

    const std::vector<double> current(6, 0.0);
    auto handle = service.move_joints(current, std::vector<double>{0.05, 0.02, -0.03, 0.01, 0.0, 0.02}, std::chrono::microseconds(1000));
    ASSERT_TRUE(handle.has_value());
    ASSERT_TRUE(service.advance(std::chrono::milliseconds(10)).has_value());
    ASSERT_TRUE(service.advance(std::chrono::milliseconds(300)).has_value());

    auto events = service.drain_events();
    ASSERT_FALSE(events.empty());

    auto snapshot = service.snapshot(std::vector<double>{0.05, 0.02, -0.03, 0.01, 0.0, 0.02});
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->axes.size(), 6u);

    auto target_pose = snapshot->tool_pose;
    auto cart_handle = service.move_cartesian(
        std::vector<double>{0.05, 0.02, -0.03, 0.01, 0.0, 0.02}, target_pose, std::chrono::microseconds(5000));
    ASSERT_TRUE(cart_handle.has_value());
}

TEST(ServiceTests, SingleAxisServiceProvidesHumanFriendlyOperations)
{
    auto axis = std::make_shared<roberta::sim::SimulatedAxisDevice>(1);
    roberta::service::SingleAxisService service(axis);

    ASSERT_TRUE(service.connect().has_value());
    ASSERT_TRUE(service.enable_drive().has_value());
    ASSERT_TRUE(service.set_motion_profile({.velocity_steps_per_second = 2000, .accel_steps_per_second_squared = 1500, .home_velocity_steps_per_second = 100}).has_value());
    ASSERT_TRUE(service.jog_steps(120).has_value());
    ASSERT_TRUE(axis->advance(std::chrono::milliseconds(100)).has_value());

    auto snapshot = service.status();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_GT(snapshot->current_position_steps, 0);

    ASSERT_TRUE(service.zero().has_value());
    snapshot = service.status();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->current_position_steps, 0);

    ASSERT_TRUE(service.disable_drive().has_value());
    snapshot = service.status();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_FALSE(snapshot->outputs_enabled);
    EXPECT_FALSE(snapshot->motor_enabled);
}

} // namespace
