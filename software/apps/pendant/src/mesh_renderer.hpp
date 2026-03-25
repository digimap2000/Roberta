#pragma once

#include "math3d.hpp"
#include "stl_loader.hpp"

#include <cstdint>

namespace roberta::pendant {

class MeshRenderer final {
public:
    struct Material final {
        Vec4 base_color;
        Vec4 detail_color;
        Vec4 response;
    };

    struct GpuMesh final {
        std::uint32_t vertex_buffer{0};
        int vertex_count{0};
    };

    MeshRenderer();
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    void set_viewport(int width, int height);
    void begin_frame(const Vec4& clear_color);
    [[nodiscard]] GpuMesh upload_mesh(const MeshData& mesh);
    void draw_mesh(const GpuMesh& mesh, const Mat4& model, const Mat4& view_projection, const Material& material,
                   const Vec3& light_direction, const Vec3& camera_position, float focus_amount, float mute_amount);

private:
    std::uint32_t program_{0};
    int viewport_width_{1};
    int viewport_height_{1};
    int view_projection_uniform_{-1};
    int model_uniform_{-1};
    int base_color_uniform_{-1};
    int detail_color_uniform_{-1};
    int material_response_uniform_{-1};
    int light_direction_uniform_{-1};
    int camera_position_uniform_{-1};
    int attention_uniform_{-1};
};

} // namespace roberta::pendant
