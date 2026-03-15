#include <roberta/model/robot_model.hpp>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <numbers>
#include <tinyxml2.h>

namespace roberta::model {
namespace {

[[nodiscard]] Mat3 identity_matrix() noexcept
{
    return Mat3{};
}

[[nodiscard]] Mat3 multiply(const Mat3& lhs, const Mat3& rhs) noexcept
{
    Mat3 out{};
    out.values.fill(0.0);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            for (int index = 0; index < 3; ++index) {
                out.values[(row * 3) + col] += lhs.values[(row * 3) + index] * rhs.values[(index * 3) + col];
            }
        }
    }
    return out;
}

[[nodiscard]] Vec3 multiply(const Mat3& lhs, const Vec3& rhs) noexcept
{
    return Vec3{
        .x = lhs.values[0] * rhs.x + lhs.values[1] * rhs.y + lhs.values[2] * rhs.z,
        .y = lhs.values[3] * rhs.x + lhs.values[4] * rhs.y + lhs.values[5] * rhs.z,
        .z = lhs.values[6] * rhs.x + lhs.values[7] * rhs.y + lhs.values[8] * rhs.z,
    };
}

[[nodiscard]] double dot(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] Vec3 normalize(const Vec3& value) noexcept
{
    const auto magnitude = std::sqrt(dot(value, value));
    if (magnitude <= 1e-9) {
        return Vec3{0.0, 0.0, 1.0};
    }
    return Vec3{value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

[[nodiscard]] Pose rpy_pose(double roll, double pitch, double yaw) noexcept
{
    const auto sr = std::sin(roll);
    const auto cr = std::cos(roll);
    const auto sp = std::sin(pitch);
    const auto cp = std::cos(pitch);
    const auto sy = std::sin(yaw);
    const auto cy = std::cos(yaw);

    Pose pose;
    pose.rotation.values = {
        cy * cp, (cy * sp * sr) - (sy * cr), (cy * sp * cr) + (sy * sr),
        sy * cp, (sy * sp * sr) + (cy * cr), (sy * sp * cr) - (cy * sr),
        -sp, cp * sr, cp * cr,
    };
    return pose;
}

[[nodiscard]] Vec3 rotation_error(const Mat3& current, const Mat3& target) noexcept
{
    const Mat3 current_t{
        .values = {
            current.values[0], current.values[3], current.values[6],
            current.values[1], current.values[4], current.values[7],
            current.values[2], current.values[5], current.values[8],
        },
    };
    const auto delta = multiply(target, current_t);
    return Vec3{
        0.5 * (delta.values[7] - delta.values[5]),
        0.5 * (delta.values[2] - delta.values[6]),
        0.5 * (delta.values[3] - delta.values[1]),
    };
}

[[nodiscard]] roberta::core::Result<Vec3> parse_xyz(const char* text)
{
    Vec3 result{};
    if (text == nullptr) {
        return result;
    }
    if (std::sscanf(text, "%lf %lf %lf", &result.x, &result.y, &result.z) != 3) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "failed to parse xyz triple"));
    }
    return result;
}

} // namespace

Pose multiply(const Pose& lhs, const Pose& rhs) noexcept
{
    return Pose{
        .rotation = multiply(lhs.rotation, rhs.rotation),
        .translation = {
            .x = lhs.translation.x + multiply(lhs.rotation, rhs.translation).x,
            .y = lhs.translation.y + multiply(lhs.rotation, rhs.translation).y,
            .z = lhs.translation.z + multiply(lhs.rotation, rhs.translation).z,
        },
    };
}

Pose rotation_about_axis(const Vec3& axis, double angle_rad) noexcept
{
    const auto n = normalize(axis);
    const auto c = std::cos(angle_rad);
    const auto s = std::sin(angle_rad);
    const auto t = 1.0 - c;

    Pose pose;
    pose.rotation.values = {
        t * n.x * n.x + c,
        t * n.x * n.y - s * n.z,
        t * n.x * n.z + s * n.y,
        t * n.x * n.y + s * n.z,
        t * n.y * n.y + c,
        t * n.y * n.z - s * n.x,
        t * n.x * n.z - s * n.y,
        t * n.y * n.z + s * n.x,
        t * n.z * n.z + c,
    };
    return pose;
}

Pose translation(const Vec3& value) noexcept
{
    return Pose{
        .rotation = identity_matrix(),
        .translation = value,
    };
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

double norm(const Vec3& value) noexcept
{
    return std::sqrt(dot(value, value));
}

RobotModel::RobotModel(RobotConfiguration configuration)
    : configuration_(std::move(configuration))
{
}

const RobotConfiguration& RobotModel::configuration() const noexcept
{
    return configuration_;
}

std::size_t RobotModel::joint_count() const noexcept
{
    return configuration_.joints.size();
}

roberta::core::Result<ForwardKinematicsResult> RobotModel::forward_kinematics(std::span<const double> joint_positions_rad) const
{
    if (joint_positions_rad.size() != configuration_.joints.size()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "joint position count mismatch"));
    }

    ForwardKinematicsResult result;
    result.link_poses.reserve(configuration_.joints.size());
    Pose current{};

    for (std::size_t index = 0; index < configuration_.joints.size(); ++index) {
        const auto& joint = configuration_.joints[index];
        current = multiply(current, joint.origin);
        current = multiply(current, rotation_about_axis(joint.axis, joint_positions_rad[index]));
        result.link_poses.push_back(current);
    }

    if (!result.link_poses.empty()) {
        result.tool_pose = result.link_poses.back();
    }
    return result;
}

roberta::core::Result<std::vector<double>> RobotModel::inverse_kinematics(
    const Pose& target,
    std::span<const double> seed_joint_positions_rad) const
{
    if (seed_joint_positions_rad.size() != configuration_.joints.size()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "ik seed count mismatch"));
    }

    std::vector<double> joints(seed_joint_positions_rad.begin(), seed_joint_positions_rad.end());
    constexpr double kEpsilon = 1e-5;
    constexpr double kStep = 1e-4;
    constexpr double kDamping = 1e-3;

    for (int iteration = 0; iteration < 80; ++iteration) {
        auto current = forward_kinematics(joints);
        if (!current) {
            return std::unexpected(current.error());
        }

        const auto translation_error = subtract(target.translation, current->tool_pose.translation);
        const auto orientation_error = rotation_error(current->tool_pose.rotation, target.rotation);

        const std::array<double, 6> error{
            translation_error.x,
            translation_error.y,
            translation_error.z,
            orientation_error.x,
            orientation_error.y,
            orientation_error.z,
        };

        double error_norm = 0.0;
        for (const auto value : error) {
            error_norm += value * value;
        }
        if (std::sqrt(error_norm) < 1e-4) {
            return joints;
        }

        std::vector<std::array<double, 6>> jacobian(configuration_.joints.size());
        for (std::size_t joint_index = 0; joint_index < joints.size(); ++joint_index) {
            auto perturbed = joints;
            perturbed[joint_index] += kStep;
            auto fk = forward_kinematics(perturbed);
            if (!fk) {
                return std::unexpected(fk.error());
            }
            const auto dt = subtract(fk->tool_pose.translation, current->tool_pose.translation);
            const auto dr = rotation_error(current->tool_pose.rotation, fk->tool_pose.rotation);
            jacobian[joint_index] = {
                dt.x / kStep,
                dt.y / kStep,
                dt.z / kStep,
                dr.x / kStep,
                dr.y / kStep,
                dr.z / kStep,
            };
        }

        std::array<std::array<double, 6>, 6> a{};
        std::array<double, 6> b{};
        for (std::size_t row = 0; row < joints.size(); ++row) {
            for (std::size_t col = 0; col < joints.size(); ++col) {
                double sum = 0.0;
                for (int error_index = 0; error_index < 6; ++error_index) {
                    sum += jacobian[row][error_index] * jacobian[col][error_index];
                }
                a[row][col] = sum + (row == col ? kDamping : 0.0);
            }
            double rhs = 0.0;
            for (int error_index = 0; error_index < 6; ++error_index) {
                rhs += jacobian[row][error_index] * error[error_index];
            }
            b[row] = rhs;
        }

        for (std::size_t pivot = 0; pivot < joints.size(); ++pivot) {
            auto diag = a[pivot][pivot];
            if (std::abs(diag) < kEpsilon) {
                diag = kEpsilon;
            }
            for (std::size_t col = pivot; col < joints.size(); ++col) {
                a[pivot][col] /= diag;
            }
            b[pivot] /= diag;

            for (std::size_t row = 0; row < joints.size(); ++row) {
                if (row == pivot) {
                    continue;
                }
                const auto factor = a[row][pivot];
                for (std::size_t col = pivot; col < joints.size(); ++col) {
                    a[row][col] -= factor * a[pivot][col];
                }
                b[row] -= factor * b[pivot];
            }
        }

        for (std::size_t joint_index = 0; joint_index < joints.size(); ++joint_index) {
            const auto& spec = configuration_.joints[joint_index];
            joints[joint_index] = std::clamp(joints[joint_index] + b[joint_index], spec.lower_limit_rad, spec.upper_limit_rad);
        }
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::unsupported, "ik did not converge"));
}

roberta::core::Result<RobotConfiguration> load_robot_from_urdf(const std::filesystem::path& path)
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "failed to load URDF"));
    }

    const auto* robot = document.FirstChildElement("robot");
    if (robot == nullptr) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "URDF missing robot element"));
    }

    RobotConfiguration configuration;
    configuration.name = robot->Attribute("name") != nullptr ? robot->Attribute("name") : "roberta";

    for (const auto* joint = robot->FirstChildElement("joint"); joint != nullptr; joint = joint->NextSiblingElement("joint")) {
        JointSpec spec;
        spec.name = joint->Attribute("name") != nullptr ? joint->Attribute("name") : "joint";
        const auto* parent = joint->FirstChildElement("parent");
        const auto* child = joint->FirstChildElement("child");
        if (parent != nullptr && parent->Attribute("link") != nullptr) {
            spec.parent_link = parent->Attribute("link");
        }
        if (child != nullptr && child->Attribute("link") != nullptr) {
            spec.child_link = child->Attribute("link");
        }

        if (configuration.base_link.empty()) {
            configuration.base_link = spec.parent_link;
        }
        configuration.tool_link = spec.child_link;

        if (const auto* origin = joint->FirstChildElement("origin")) {
            auto xyz = parse_xyz(origin->Attribute("xyz"));
            if (!xyz) {
                return std::unexpected(xyz.error());
            }
            auto rpy = parse_xyz(origin->Attribute("rpy"));
            if (!rpy) {
                return std::unexpected(rpy.error());
            }
            spec.origin = rpy_pose(rpy->x, rpy->y, rpy->z);
            spec.origin.translation = *xyz;
        }

        if (const auto* axis = joint->FirstChildElement("axis")) {
            auto axis_value = parse_xyz(axis->Attribute("xyz"));
            if (!axis_value) {
                return std::unexpected(axis_value.error());
            }
            spec.axis = *axis_value;
        }

        if (const auto* limit = joint->FirstChildElement("limit")) {
            limit->QueryDoubleAttribute("lower", &spec.lower_limit_rad);
            limit->QueryDoubleAttribute("upper", &spec.upper_limit_rad);
            limit->QueryDoubleAttribute("velocity", &spec.velocity_limit_rad_s);
        }

        configuration.joints.push_back(spec);
    }

    return configuration;
}

} // namespace roberta::model
