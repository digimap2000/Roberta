#pragma once

#include "math3d.hpp"

#include <filesystem>
#include <vector>

namespace roberta::pendant {

struct MeshVertex final {
    Vec3 position;
    Vec3 normal;
};

struct MeshData final {
    std::vector<MeshVertex> vertices;
};

[[nodiscard]] MeshData load_stl_mesh(const std::filesystem::path& path, float uniform_scale);

} // namespace roberta::pendant
