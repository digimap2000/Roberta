#include "stl_loader.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

namespace roberta::pendant {
namespace {

template <typename T>
T read_value(std::ifstream& stream)
{
    T value{};
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!stream) {
        throw std::runtime_error("unexpected end of STL file");
    }
    return value;
}

[[nodiscard]] Vec3 read_vec3(std::ifstream& stream)
{
    return Vec3{
        read_value<float>(stream),
        read_value<float>(stream),
        read_value<float>(stream),
    };
}

} // namespace

MeshData load_stl_mesh(const std::filesystem::path& path, float uniform_scale)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open STL mesh: " + path.string());
    }

    stream.seekg(80, std::ios::beg);
    if (!stream) {
        throw std::runtime_error("failed to seek STL header: " + path.string());
    }

    const auto triangle_count = read_value<std::uint32_t>(stream);

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(triangle_count) * 3U);

    for (std::uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        auto normal = read_vec3(stream);
        auto vertex_a = read_vec3(stream);
        auto vertex_b = read_vec3(stream);
        auto vertex_c = read_vec3(stream);
        (void)read_value<std::uint16_t>(stream);

        vertex_a = vertex_a * uniform_scale;
        vertex_b = vertex_b * uniform_scale;
        vertex_c = vertex_c * uniform_scale;

        if (length(normal) <= 0.00001F) {
            normal = normalize(cross(vertex_b - vertex_a, vertex_c - vertex_a));
        } else {
            normal = normalize(normal);
        }

        mesh.vertices.push_back(MeshVertex{vertex_a, normal});
        mesh.vertices.push_back(MeshVertex{vertex_b, normal});
        mesh.vertices.push_back(MeshVertex{vertex_c, normal});
    }

    return mesh;
}

} // namespace roberta::pendant
