#include "robot_view.hpp"

#include "platform_gl.hpp"
#include "stl_loader.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <string_view>

namespace roberta::pendant {
namespace {

constexpr Vec3 k_camera_target{-0.15F, 0.20F, 0.75F};
constexpr float k_min_orbit_pitch{-0.15F};
constexpr float k_max_orbit_pitch{1.20F};

struct LinkDefinition final {
    std::string_view mesh_name;
    MeshRenderer::Material material;
    int parent_index;
    Vec3 joint_origin;
    Vec3 joint_axis;
    int joint_index;
};

constexpr std::array<LinkDefinition, 7> k_link_definitions{{
    LinkDefinition{"base_link.stl", {{0.20F, 0.22F, 0.25F, 1.0F}, {0.43F, 0.49F, 0.54F, 1.0F},
                                     {0.72F, 0.20F, 0.05F, 0.42F}},
                   -1, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, -1},
    LinkDefinition{"shoulder_link.stl", {{0.72F, 0.72F, 0.69F, 1.0F}, {0.84F, 0.83F, 0.79F, 1.0F},
                                         {0.48F, 0.22F, 0.03F, 0.22F}},
                   0, {0.0F, 0.0F, 0.268F}, {0.0F, 0.0F, 1.0F}, 0},
    LinkDefinition{"upperarm_link.stl", {{0.60F, 0.65F, 0.70F, 1.0F}, {0.78F, 0.82F, 0.86F, 1.0F},
                                         {0.38F, 0.28F, 0.04F, 0.18F}},
                   1, {-0.300F, 0.105F, 0.332F}, {0.0F, 1.0F, 0.0F}, 1},
    LinkDefinition{"forearm_link.stl", {{0.46F, 0.49F, 0.53F, 1.0F}, {0.66F, 0.71F, 0.76F, 1.0F},
                                        {0.42F, 0.34F, 0.05F, 0.26F}},
                   2, {0.0F, 0.075F, 0.700F}, {0.0F, 1.0F, 0.0F}, 2},
    LinkDefinition{"wrista_link.stl", {{0.30F, 0.32F, 0.35F, 1.0F}, {0.50F, 0.56F, 0.60F, 1.0F},
                                       {0.64F, 0.18F, 0.05F, 0.30F}},
                   3, {-0.238F, -0.180F, 0.185F}, {1.0F, 0.0F, 0.0F}, 3},
    LinkDefinition{"wristb_link.stl", {{0.56F, 0.60F, 0.61F, 1.0F}, {0.73F, 0.79F, 0.80F, 1.0F},
                                       {0.30F, 0.38F, 0.04F, 0.20F}},
                   4, {-0.386F, 0.0F, -0.029F}, {0.0F, 0.0F, 1.0F}, 4},
    LinkDefinition{"flange_link.stl", {{0.68F, 0.67F, 0.63F, 1.0F}, {0.84F, 0.81F, 0.72F, 1.0F},
                                       {0.22F, 0.46F, 0.02F, 0.18F}},
                   5, {-0.104F, 0.0F, 0.029F}, {1.0F, 0.0F, 0.0F}, 5},
}};

[[nodiscard]] std::filesystem::path append_if_exists(const std::filesystem::path& root)
{
    if (std::filesystem::exists(root / "meshes" / "base_link.stl")) {
        return root;
    }
    return {};
}

} // namespace

RobotView::RobotView(MeshRenderer& renderer)
    : asset_root_(resolve_asset_root())
{
    links_.reserve(k_link_definitions.size());

    for (const auto& definition : k_link_definitions) {
        const auto mesh_path = asset_root_ / "meshes" / definition.mesh_name;
        auto mesh = load_stl_mesh(mesh_path, 0.001F);
        links_.push_back(RobotLink{
            .mesh = renderer.upload_mesh(mesh),
            .material = definition.material,
            .parent_index = definition.parent_index,
            .joint_origin = definition.joint_origin,
            .joint_axis = definition.joint_axis,
            .joint_index = definition.joint_index,
        });
    }
}

RobotView::~RobotView()
{
    for (const auto& link : links_) {
        if (link.mesh.vertex_buffer != 0U) {
            auto buffer = static_cast<GLuint>(link.mesh.vertex_buffer);
            glDeleteBuffers(1, &buffer);
        }
    }
}

std::filesystem::path RobotView::resolve_asset_root()
{
    if (const char* asset_root = std::getenv("ROBERTA_ASSET_ROOT"); asset_root != nullptr) {
        if (auto resolved = append_if_exists(asset_root); !resolved.empty()) {
            return resolved;
        }
    }

    const auto current_directory = std::filesystem::current_path();
    if (auto resolved = append_if_exists(current_directory / "urdf"); !resolved.empty()) {
        return resolved;
    }
    if (auto resolved = append_if_exists(current_directory / "software" / "urdf"); !resolved.empty()) {
        return resolved;
    }

    if (char* base_path = SDL_GetBasePath(); base_path != nullptr) {
        const std::filesystem::path executable_root(base_path);
        SDL_free(base_path);

        if (auto resolved = append_if_exists(executable_root / "urdf"); !resolved.empty()) {
            return resolved;
        }
        if (auto resolved = append_if_exists(executable_root / ".." / "urdf"); !resolved.empty()) {
            return resolved;
        }
    }

    throw std::runtime_error(
        "could not find robot assets; copy the urdf directory next to the binary or set ROBERTA_ASSET_ROOT");
}

void RobotView::handle_keydown(int keycode)
{
    switch (keycode) {
    case SDLK_LEFT:
    case SDLK_KP_4:
        orbit_yaw_ -= 0.12F;
        break;
    case SDLK_RIGHT:
    case SDLK_KP_6:
        orbit_yaw_ += 0.12F;
        break;
    case SDLK_UP:
    case SDLK_KP_8:
        orbit_pitch_ = std::clamp(orbit_pitch_ + 0.08F, k_min_orbit_pitch, k_max_orbit_pitch);
        break;
    case SDLK_DOWN:
    case SDLK_KP_2:
        orbit_pitch_ = std::clamp(orbit_pitch_ - 0.08F, k_min_orbit_pitch, k_max_orbit_pitch);
        break;
    case SDLK_PERIOD:
    case SDLK_KP_PERIOD:
        camera_distance_ = std::max(1.4F, camera_distance_ - 0.15F);
        break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
        camera_distance_ = std::min(5.0F, camera_distance_ + 0.15F);
        break;
    case SDLK_SPACE:
        auto_motion_ = !auto_motion_;
        break;
    case SDLK_r:
        orbit_yaw_ = 0.75F;
        orbit_pitch_ = 0.35F;
        camera_distance_ = 2.8F;
        auto_motion_ = true;
        break;
    default:
        break;
    }
}

void RobotView::update(float delta_seconds)
{
    const auto dt = std::max(delta_seconds, 0.0F);
    time_seconds_ += dt;

    if (!auto_motion_) {
        return;
    }

    joint_angles_[0] = std::sin(time_seconds_ * 0.45F) * 0.55F;
    joint_angles_[1] = std::sin(time_seconds_ * 0.40F + 0.7F) * 0.50F;
    joint_angles_[2] = std::sin(time_seconds_ * 0.55F + 1.3F) * 0.75F;
    joint_angles_[3] = std::sin(time_seconds_ * 0.80F + 0.2F) * 0.60F;
    joint_angles_[4] = std::sin(time_seconds_ * 1.10F + 0.9F) * 0.75F;
    joint_angles_[5] = std::sin(time_seconds_ * 0.95F + 1.6F) * 0.55F;
}

std::array<Mat4, 7> RobotView::compute_link_transforms() const
{
    std::array<Mat4, 7> transforms{};
    transforms[0] = identity_matrix();

    for (std::size_t index = 1; index < links_.size(); ++index) {
        const auto& link = links_[index];
        const auto parent_transform = transforms[static_cast<std::size_t>(link.parent_index)];
        const auto translation = translation_matrix(link.joint_origin);
        const auto rotation = rotation_matrix(link.joint_axis, joint_angles_[static_cast<std::size_t>(link.joint_index)]);
        transforms[index] = multiply(parent_transform, multiply(translation, rotation));
    }

    return transforms;
}

void RobotView::draw(MeshRenderer& renderer, int surface_width, int surface_height) const
{
    const auto world_up = Vec3{0.0F, 0.0F, 1.0F};
    const auto aspect_ratio =
        static_cast<float>(std::max(surface_width, 1)) / static_cast<float>(std::max(surface_height, 1));
    const auto projection =
        perspective_matrix(std::numbers::pi_v<float> / 4.2F, aspect_ratio, 0.05F, 12.0F);

    const auto eye = Vec3{
        k_camera_target.x + (camera_distance_ * std::cos(orbit_pitch_) * std::cos(orbit_yaw_)),
        k_camera_target.y + (camera_distance_ * std::cos(orbit_pitch_) * std::sin(orbit_yaw_)),
        k_camera_target.z + (camera_distance_ * std::sin(orbit_pitch_)),
    };
    const auto camera_forward = normalize(k_camera_target - eye);
    const auto camera_right = normalize(cross(camera_forward, world_up));
    const auto camera_up = cross(camera_right, camera_forward);
    const auto light_origin = eye + (camera_up * (camera_distance_ * 0.35F));
    const auto light_direction = normalize(k_camera_target - light_origin);

    const auto view = look_at_matrix(eye, k_camera_target, world_up);
    const auto view_projection = multiply(projection, view);

    const auto transforms = compute_link_transforms();
    for (std::size_t index = 0; index < links_.size(); ++index) {
        renderer.draw_mesh(links_[index].mesh, transforms[index], view_projection, links_[index].material,
                           light_direction, eye);
    }
}

} // namespace roberta::pendant
