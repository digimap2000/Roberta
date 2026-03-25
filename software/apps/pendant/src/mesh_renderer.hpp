#pragma once

#include "math3d.hpp"
#include "stl_loader.hpp"

#include <cstdint>

namespace roberta::pendant {

class MeshRenderer final {
public:
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
    void draw_mesh(const GpuMesh& mesh, const Mat4& model, const Mat4& view_projection, const Vec4& color);

private:
    std::uint32_t program_{0};
    int viewport_width_{1};
    int viewport_height_{1};
    int view_projection_uniform_{-1};
    int model_uniform_{-1};
    int color_uniform_{-1};
    int light_direction_uniform_{-1};
};

} // namespace roberta::pendant
