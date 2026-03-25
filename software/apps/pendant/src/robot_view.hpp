#pragma once

#include "line_renderer.hpp"
#include "math3d.hpp"
#include "mesh_renderer.hpp"
#include "text_renderer.hpp"

#include <array>
#include <filesystem>
#include <vector>

namespace roberta::pendant {

class RobotView final {
public:
    explicit RobotView(MeshRenderer& renderer);
    ~RobotView();

    RobotView(const RobotView&) = delete;
    RobotView& operator=(const RobotView&) = delete;

    void handle_keydown(int keycode);
    void update(float delta_seconds);
    void draw(MeshRenderer& renderer, LineRenderer& line_renderer, TextRenderer& text_renderer, int surface_width,
              int surface_height) const;
    [[nodiscard]] int selected_axis() const noexcept;

private:
    struct RobotLink final {
        MeshRenderer::GpuMesh mesh;
        MeshRenderer::Material material;
        int parent_index;
        Vec3 joint_origin;
        Vec3 joint_axis;
        int joint_index;
    };

    [[nodiscard]] static std::filesystem::path resolve_asset_root();
    [[nodiscard]] std::array<Mat4, 7> compute_link_transforms() const;

    std::filesystem::path asset_root_;
    std::vector<RobotLink> links_;
    std::array<float, 6> joint_angles_{};
    std::array<float, 7> link_focus_blends_{};
    std::array<float, 7> link_mute_blends_{};
    std::array<float, 6> axis_annotation_blends_{};
    float time_seconds_{0.0F};
    float orbit_yaw_{0.75F};
    float orbit_pitch_{0.35F};
    float camera_distance_{2.8F};
    bool auto_motion_{true};
    int selected_axis_{-1};
};

} // namespace roberta::pendant
