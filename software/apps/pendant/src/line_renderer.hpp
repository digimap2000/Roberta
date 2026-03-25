#pragma once

#include "math3d.hpp"

#include <cstdint>
#include <vector>

namespace roberta::pendant {

class LineRenderer final {
public:
    struct Vertex final {
        Vec3 position;
        Vec4 color;
    };

    LineRenderer();
    ~LineRenderer();

    LineRenderer(const LineRenderer&) = delete;
    LineRenderer& operator=(const LineRenderer&) = delete;

    void draw_lines(const std::vector<Vertex>& vertices, const Mat4& view_projection, bool depth_test);

private:
    std::uint32_t program_{0};
    std::uint32_t vertex_buffer_{0};
    int view_projection_uniform_{-1};
};

} // namespace roberta::pendant
