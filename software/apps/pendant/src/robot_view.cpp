#include "robot_view.hpp"

#include "platform_gl.hpp"
#include "stl_loader.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace roberta::pendant {
namespace {

constexpr Vec3 k_camera_target{-0.15F, 0.20F, 0.75F};
constexpr float k_min_orbit_pitch{-0.15F};
constexpr float k_max_orbit_pitch{1.20F};
constexpr float k_default_orbit_yaw{0.75F + std::numbers::pi_v<float>};
constexpr float k_default_orbit_pitch{0.35F};
constexpr float k_default_camera_distance{3.1F};
constexpr std::array<float, 6> k_axis_annotation_radii{{0.44F, 0.25F, 0.24F, 0.20F, 0.17F, 0.15F}};

struct LinkDefinition final {
    std::string_view mesh_name;
    MeshRenderer::Material material;
    int parent_index;
    Vec3 joint_origin;
    Vec3 joint_axis;
    int joint_index;
};

constexpr std::array<LinkDefinition, 7> k_link_definitions{{
    LinkDefinition{"base_link.stl", {{0.50F, 0.07F, 0.06F, 1.0F}, {0.71F, 0.18F, 0.13F, 1.0F},
                                     {0.76F, 0.22F, 0.04F, 0.44F}},
                   -1, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, -1},
    LinkDefinition{"shoulder_link.stl", {{0.58F, 0.08F, 0.07F, 1.0F}, {0.77F, 0.20F, 0.15F, 1.0F},
                                         {0.46F, 0.24F, 0.03F, 0.22F}},
                   0, {0.0F, 0.0F, 0.268F}, {0.0F, 0.0F, 1.0F}, 0},
    LinkDefinition{"upperarm_link.stl", {{0.61F, 0.09F, 0.08F, 1.0F}, {0.81F, 0.22F, 0.16F, 1.0F},
                                         {0.34F, 0.30F, 0.04F, 0.18F}},
                   1, {-0.300F, 0.105F, 0.332F}, {0.0F, 1.0F, 0.0F}, 1},
    LinkDefinition{"forearm_link.stl", {{0.54F, 0.07F, 0.06F, 1.0F}, {0.74F, 0.18F, 0.13F, 1.0F},
                                        {0.38F, 0.34F, 0.05F, 0.26F}},
                   2, {0.0F, 0.075F, 0.700F}, {0.0F, 1.0F, 0.0F}, 2},
    LinkDefinition{"wrista_link.stl", {{0.53F, 0.08F, 0.07F, 1.0F}, {0.75F, 0.20F, 0.15F, 1.0F},
                                       {0.62F, 0.20F, 0.05F, 0.30F}},
                   3, {-0.238F, -0.180F, 0.185F}, {1.0F, 0.0F, 0.0F}, 3},
    LinkDefinition{"wristb_link.stl", {{0.60F, 0.08F, 0.07F, 1.0F}, {0.80F, 0.19F, 0.15F, 1.0F},
                                       {0.30F, 0.38F, 0.04F, 0.20F}},
                   4, {-0.386F, 0.0F, -0.029F}, {0.0F, 0.0F, 1.0F}, 4},
    LinkDefinition{"flange_link.stl", {{0.63F, 0.10F, 0.08F, 1.0F}, {0.83F, 0.24F, 0.17F, 1.0F},
                                       {0.20F, 0.48F, 0.03F, 0.18F}},
                   5, {-0.104F, 0.0F, 0.029F}, {1.0F, 0.0F, 0.0F}, 5},
}};

[[nodiscard]] std::filesystem::path append_if_exists(const std::filesystem::path& root)
{
    if (std::filesystem::exists(root / "meshes" / "base_link.stl")) {
        return root;
    }
    return {};
}

[[nodiscard]] Vec3 perpendicular_hint(const Vec3& axis)
{
    if (std::abs(axis.z) < 0.82F) {
        return Vec3{0.0F, 0.0F, 1.0F};
    }
    return Vec3{1.0F, 0.0F, 0.0F};
}

[[nodiscard]] Vec3 project_onto_plane(const Vec3& value, const Vec3& plane_normal)
{
    return value - (plane_normal * dot(value, plane_normal));
}

[[nodiscard]] Vec4 scale_alpha(const Vec4& color, float alpha_scale)
{
    return Vec4{color.x, color.y, color.z, color.w * alpha_scale};
}

void add_segment(std::vector<LineRenderer::Vertex>& vertices, const Vec3& start, const Vec3& end, const Vec4& color)
{
    vertices.push_back(LineRenderer::Vertex{start, color});
    vertices.push_back(LineRenderer::Vertex{end, color});
}

void add_ring(std::vector<LineRenderer::Vertex>& vertices, const Vec3& center, const Vec3& basis_u, const Vec3& basis_v,
              float radius, int segments, const Vec4& color)
{
    const auto segment_count = std::max(segments, 3);
    for (int index = 0; index < segment_count; ++index) {
        const auto angle_a = (static_cast<float>(index) / static_cast<float>(segment_count)) * (2.0F * std::numbers::pi_v<float>);
        const auto angle_b =
            (static_cast<float>(index + 1) / static_cast<float>(segment_count)) * (2.0F * std::numbers::pi_v<float>);
        const auto point_a = center + (basis_u * (std::cos(angle_a) * radius)) + (basis_v * (std::sin(angle_a) * radius));
        const auto point_b = center + (basis_u * (std::cos(angle_b) * radius)) + (basis_v * (std::sin(angle_b) * radius));
        add_segment(vertices, point_a, point_b, color);
    }
}

void add_arc(std::vector<LineRenderer::Vertex>& vertices, const Vec3& center, const Vec3& basis_u, const Vec3& basis_v,
             float radius, float start_angle, float end_angle, int segments, const Vec4& color)
{
    const auto segment_count = std::max(segments, 1);
    for (int index = 0; index < segment_count; ++index) {
        const auto t0 = static_cast<float>(index) / static_cast<float>(segment_count);
        const auto t1 = static_cast<float>(index + 1) / static_cast<float>(segment_count);
        const auto angle_a = start_angle + ((end_angle - start_angle) * t0);
        const auto angle_b = start_angle + ((end_angle - start_angle) * t1);
        const auto point_a = center + (basis_u * (std::cos(angle_a) * radius)) + (basis_v * (std::sin(angle_a) * radius));
        const auto point_b = center + (basis_u * (std::cos(angle_b) * radius)) + (basis_v * (std::sin(angle_b) * radius));
        add_segment(vertices, point_a, point_b, color);
    }
}

[[nodiscard]] bool project_to_screen(const Mat4& view_projection, const Vec3& world_position, int surface_width,
                                     int surface_height, float& out_x, float& out_y)
{
    const auto clip = transform(view_projection, Vec4{world_position.x, world_position.y, world_position.z, 1.0F});
    if (clip.w <= 0.00001F) {
        return false;
    }

    const auto inverse_w = 1.0F / clip.w;
    const auto ndc_x = clip.x * inverse_w;
    const auto ndc_y = clip.y * inverse_w;
    const auto ndc_z = clip.z * inverse_w;

    if (ndc_z < -1.0F || ndc_z > 1.0F) {
        return false;
    }

    out_x = (ndc_x * 0.5F + 0.5F) * static_cast<float>(surface_width);
    out_y = (1.0F - (ndc_y * 0.5F + 0.5F)) * static_cast<float>(surface_height);
    return true;
}

} // namespace

RobotView::RobotView(MeshRenderer& renderer)
    : asset_root_(resolve_asset_root())
{
    orbit_yaw_ = k_default_orbit_yaw;
    orbit_pitch_ = k_default_orbit_pitch;
    camera_distance_ = k_default_camera_distance;

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
    case SDLK_1:
        selected_axis_ = selected_axis_ == 0 ? -1 : 0;
        break;
    case SDLK_2:
        selected_axis_ = selected_axis_ == 1 ? -1 : 1;
        break;
    case SDLK_3:
        selected_axis_ = selected_axis_ == 2 ? -1 : 2;
        break;
    case SDLK_4:
        selected_axis_ = selected_axis_ == 3 ? -1 : 3;
        break;
    case SDLK_5:
        selected_axis_ = selected_axis_ == 4 ? -1 : 4;
        break;
    case SDLK_6:
        selected_axis_ = selected_axis_ == 5 ? -1 : 5;
        break;
    case SDLK_SPACE:
        auto_motion_ = !auto_motion_;
        break;
    case SDLK_r:
        orbit_yaw_ = k_default_orbit_yaw;
        orbit_pitch_ = k_default_orbit_pitch;
        camera_distance_ = k_default_camera_distance;
        auto_motion_ = true;
        break;
    default:
        break;
    }
}

int RobotView::selected_axis() const noexcept
{
    return selected_axis_;
}

void RobotView::update(float delta_seconds)
{
    const auto dt = std::max(delta_seconds, 0.0F);
    time_seconds_ += dt;

    const auto blend_t = 1.0F - std::exp(-dt * 10.0F);
    std::array<float, 7> target_link_focus{};
    std::array<float, 7> target_link_mute{};
    std::array<float, 6> target_axis_annotation{};

    if (selected_axis_ >= 0 && selected_axis_ < static_cast<int>(axis_annotation_blends_.size())) {
        const auto active_axis_index = static_cast<std::size_t>(selected_axis_);
        const auto active_joint_link = links_[active_axis_index + 1];
        target_link_focus[static_cast<std::size_t>(active_joint_link.parent_index)] = 1.0F;
        target_link_focus[active_axis_index + 1] = 1.0F;
        target_axis_annotation[active_axis_index] = 1.0F;

        for (std::size_t link_index = 0; link_index < target_link_mute.size(); ++link_index) {
            if (target_link_focus[link_index] < 0.5F) {
                target_link_mute[link_index] = 1.0F;
            }
        }
    }

    for (std::size_t link_index = 0; link_index < link_focus_blends_.size(); ++link_index) {
        link_focus_blends_[link_index] += (target_link_focus[link_index] - link_focus_blends_[link_index]) * blend_t;
        link_mute_blends_[link_index] += (target_link_mute[link_index] - link_mute_blends_[link_index]) * blend_t;
    }

    for (std::size_t axis_index = 0; axis_index < axis_annotation_blends_.size(); ++axis_index) {
        axis_annotation_blends_[axis_index] +=
            (target_axis_annotation[axis_index] - axis_annotation_blends_[axis_index]) * blend_t;
    }

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

void RobotView::draw(MeshRenderer& renderer, LineRenderer& line_renderer, TextRenderer& text_renderer, int surface_width,
                     int surface_height) const
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
                           light_direction, eye, link_focus_blends_[index], link_mute_blends_[index]);
    }

    std::vector<LineRenderer::Vertex> annotation_vertices;
    annotation_vertices.reserve(720);

    float dominant_blend = 0.0F;
    int dominant_axis = -1;
    Vec3 dominant_label_anchor{};

    for (std::size_t axis_index = 0; axis_index < axis_annotation_blends_.size(); ++axis_index) {
        const auto blend = axis_annotation_blends_[axis_index];
        if (blend <= 0.02F) {
            continue;
        }

        const auto& joint_link = links_[axis_index + 1];
        const auto parent_transform = transforms[static_cast<std::size_t>(joint_link.parent_index)];
        const auto child_transform = transforms[axis_index + 1];
        const auto joint_origin_world = transform_point(parent_transform, joint_link.joint_origin);
        const auto joint_axis_world = normalize(transform_direction(parent_transform, joint_link.joint_axis));
        const auto rotating_reference_world =
            transform_direction(child_transform, perpendicular_hint(joint_link.joint_axis));
        auto basis_u = normalize(project_onto_plane(rotating_reference_world, joint_axis_world));
        if (length(basis_u) <= 0.00001F) {
            basis_u = normalize(cross(joint_axis_world, perpendicular_hint(joint_axis_world)));
        }
        const auto basis_v = normalize(cross(joint_axis_world, basis_u));
        const auto annotation_radius = k_axis_annotation_radii[axis_index];
        const auto plate_offset = joint_axis_world * (annotation_radius * 0.11F);
        const auto label_anchor =
            joint_origin_world + (basis_u * (annotation_radius * 1.72F)) + (basis_v * (annotation_radius * 0.78F));

        const auto bright = scale_alpha(Vec4{1.0F, 1.0F, 1.0F, 1.0F}, blend);
        const auto mid = scale_alpha(Vec4{0.97F, 0.99F, 1.0F, 0.70F}, blend);
        const auto soft = scale_alpha(Vec4{0.94F, 0.98F, 1.0F, 0.34F}, blend);
        const auto faint = scale_alpha(Vec4{0.94F, 0.98F, 1.0F, 0.18F}, blend);

        add_ring(annotation_vertices, joint_origin_world - plate_offset, basis_u, basis_v, annotation_radius * 0.98F, 56, mid);
        add_ring(annotation_vertices, joint_origin_world + plate_offset, basis_u, basis_v, annotation_radius * 0.98F, 56, bright);
        add_ring(annotation_vertices, joint_origin_world, basis_u, basis_v, annotation_radius * 0.74F, 40, soft);
        add_ring(annotation_vertices, joint_origin_world, basis_u, basis_v, annotation_radius * 1.18F, 64, faint);
        add_arc(annotation_vertices, joint_origin_world, basis_u, basis_v, annotation_radius * 1.28F, 0.10F,
                0.74F * std::numbers::pi_v<float>, 24, soft);
        add_arc(annotation_vertices, joint_origin_world, basis_u, basis_v, annotation_radius * 1.28F,
                0.92F * std::numbers::pi_v<float>, 1.36F * std::numbers::pi_v<float>, 14, faint);
        add_arc(annotation_vertices, joint_origin_world, basis_u, basis_v, annotation_radius * 1.28F,
                1.50F * std::numbers::pi_v<float>, 1.92F * std::numbers::pi_v<float>, 16, soft);

        for (int tick_index = 0; tick_index < 24; ++tick_index) {
            const auto angle = (static_cast<float>(tick_index) / 24.0F) * (2.0F * std::numbers::pi_v<float>);
            const auto direction = (basis_u * std::cos(angle)) + (basis_v * std::sin(angle));
            const auto inner = joint_origin_world + (direction * (annotation_radius * (tick_index % 6 == 0 ? 1.01F : 1.07F)));
            const auto outer =
                joint_origin_world + (direction * (annotation_radius * (tick_index % 6 == 0 ? 1.34F : 1.18F)));
            add_segment(annotation_vertices, inner, outer, tick_index % 6 == 0 ? mid : faint);
        }

        for (const Vec3 direction : {basis_u, basis_v, basis_u * -1.0F, basis_v * -1.0F}) {
            add_segment(annotation_vertices, joint_origin_world + (direction * (annotation_radius * 0.24F)),
                        joint_origin_world + (direction * (annotation_radius * 0.62F)), soft);
            add_segment(annotation_vertices, joint_origin_world + plate_offset + (direction * (annotation_radius * 0.98F)),
                        joint_origin_world - plate_offset + (direction * (annotation_radius * 0.98F)), soft);
            add_segment(annotation_vertices, joint_origin_world + plate_offset + (direction * (annotation_radius * 1.18F)),
                        joint_origin_world - plate_offset + (direction * (annotation_radius * 1.18F)), faint);
        }

        for (int frame_index = 0; frame_index < 6; ++frame_index) {
            const auto angle = (static_cast<float>(frame_index) / 6.0F) * (2.0F * std::numbers::pi_v<float>);
            const auto direction = (basis_u * std::cos(angle)) + (basis_v * std::sin(angle));
            const auto brace_a = joint_origin_world + plate_offset + (direction * (annotation_radius * 0.76F));
            const auto brace_b = joint_origin_world - plate_offset + (direction * (annotation_radius * 0.76F));
            const auto cage_a = joint_origin_world + plate_offset + (direction * (annotation_radius * 1.18F));
            const auto cage_b = joint_origin_world - plate_offset + (direction * (annotation_radius * 1.18F));
            add_segment(annotation_vertices, brace_a, cage_a, faint);
            add_segment(annotation_vertices, brace_b, cage_b, faint);
            add_segment(annotation_vertices, cage_a, cage_b, faint);
        }

        add_segment(annotation_vertices, joint_origin_world - (joint_axis_world * (annotation_radius * 0.82F)),
                    joint_origin_world + (joint_axis_world * (annotation_radius * 0.82F)), soft);
        add_segment(annotation_vertices, joint_origin_world + (basis_u * (annotation_radius * 1.18F)), label_anchor, mid);

        if (blend > dominant_blend) {
            dominant_blend = blend;
            dominant_axis = static_cast<int>(axis_index);
            dominant_label_anchor = label_anchor;
        }
    }

    line_renderer.draw_lines(annotation_vertices, view_projection, true);

    if (dominant_axis >= 0 && dominant_blend > 0.15F) {
        float label_x = 0.0F;
        float label_y = 0.0F;
        if (project_to_screen(view_projection, dominant_label_anchor, surface_width, surface_height, label_x, label_y)) {
            const TextStyle label_shadow_style{
                .font = FontFace::mono,
                .pixel_height = 18.0F,
                .line_spacing = 1.0F,
                .color = Color{0.01F, 0.01F, 0.02F, 0.75F * dominant_blend},
            };
            const TextStyle label_style{
                .font = FontFace::mono,
                .pixel_height = 18.0F,
                .line_spacing = 1.0F,
                .color = Color{0.96F, 0.98F, 1.0F, 0.96F * dominant_blend},
            };

            const auto label = std::string("AXIS ") + std::to_string(dominant_axis + 1) + " // JOINT PLANE";
            text_renderer.draw_text(label, label_x + 1.0F, label_y + 1.0F, label_shadow_style);
            text_renderer.draw_text(label, label_x, label_y, label_style);
        }
    }
}

} // namespace roberta::pendant
