#pragma once

#include "math3d.hpp"
#include "mesh_renderer.hpp"

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
    void draw(MeshRenderer& renderer, int surface_width, int surface_height) const;

private:
    struct RobotLink final {
        MeshRenderer::GpuMesh mesh;
        Vec4 color;
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
    float time_seconds_{0.0F};
    float orbit_yaw_{0.75F};
    float orbit_pitch_{0.35F};
    float camera_distance_{2.8F};
    bool auto_motion_{true};
};

} // namespace roberta::pendant
