#pragma once

#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <roberta/core/error.hpp>

namespace roberta::model {

struct Vec3 final {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct Mat3 final {
    std::array<double, 9> values{
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    };
};

struct Pose final {
    Mat3 rotation{};
    Vec3 translation{};
};

struct JointSpec final {
    std::string name;
    std::string parent_link;
    std::string child_link;
    Pose origin{};
    Vec3 axis{0.0, 0.0, 1.0};
    double lower_limit_rad{-3.141592653589793};
    double upper_limit_rad{3.141592653589793};
    double velocity_limit_rad_s{1.5};
};

struct RobotConfiguration final {
    std::string name;
    std::vector<JointSpec> joints;
    std::string base_link;
    std::string tool_link;
};

struct ForwardKinematicsResult final {
    std::vector<Pose> link_poses;
    Pose tool_pose{};
};

struct AxisCalibration final {
    std::uint32_t axis_id{0};
    double steps_per_degree{10.0};
};

class RobotModel final {
public:
    explicit RobotModel(RobotConfiguration configuration);

    [[nodiscard]] const RobotConfiguration& configuration() const noexcept;
    [[nodiscard]] std::size_t joint_count() const noexcept;
    [[nodiscard]] roberta::core::Result<ForwardKinematicsResult> forward_kinematics(std::span<const double> joint_positions_rad) const;
    [[nodiscard]] roberta::core::Result<std::vector<double>> inverse_kinematics(
        const Pose& target,
        std::span<const double> seed_joint_positions_rad) const;

private:
    RobotConfiguration configuration_;
};

[[nodiscard]] roberta::core::Result<RobotConfiguration> load_robot_from_urdf(const std::filesystem::path& path);
[[nodiscard]] Pose multiply(const Pose& lhs, const Pose& rhs) noexcept;
[[nodiscard]] Pose rotation_about_axis(const Vec3& axis, double angle_rad) noexcept;
[[nodiscard]] Pose translation(const Vec3& value) noexcept;
[[nodiscard]] Vec3 subtract(const Vec3& lhs, const Vec3& rhs) noexcept;
[[nodiscard]] double norm(const Vec3& value) noexcept;

} // namespace roberta::model
