#include "stl_loader.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace roberta::pendant {
namespace {

constexpr char k_preprocessed_mesh_magic[8] = {'R', 'B', 'T', 'M', 'E', 'S', 'H', '1'};
constexpr std::uint32_t k_preprocessed_mesh_version = 1U;

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

[[nodiscard]] std::filesystem::path preprocessed_mesh_path(const std::filesystem::path& path)
{
    auto result = path;
    result.replace_extension(".rmesh");
    return result;
}

[[nodiscard]] MeshData load_preprocessed_mesh(const std::filesystem::path& path, float uniform_scale)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open preprocessed mesh: " + path.string());
    }

    char magic[8]{};
    stream.read(magic, static_cast<std::streamsize>(sizeof(magic)));
    if (!stream || std::memcmp(magic, k_preprocessed_mesh_magic, sizeof(magic)) != 0) {
        throw std::runtime_error("invalid preprocessed mesh header: " + path.string());
    }

    const auto version = read_value<std::uint32_t>(stream);
    if (version != k_preprocessed_mesh_version) {
        throw std::runtime_error("unsupported preprocessed mesh version: " + path.string());
    }

    const auto vertex_count = read_value<std::uint32_t>(stream);
    const auto baked_scale = read_value<float>(stream);

    MeshData mesh;
    mesh.vertices.resize(static_cast<std::size_t>(vertex_count));
    stream.read(reinterpret_cast<char*>(mesh.vertices.data()),
                static_cast<std::streamsize>(mesh.vertices.size() * sizeof(MeshVertex)));
    if (!stream) {
        throw std::runtime_error("unexpected end of preprocessed mesh: " + path.string());
    }

    if (std::abs(baked_scale - uniform_scale) > 0.000001F && std::abs(baked_scale) > 0.000001F) {
        const auto scale = uniform_scale / baked_scale;
        for (auto& vertex : mesh.vertices) {
            vertex.position = vertex.position * scale;
        }
    }

    return mesh;
}

} // namespace

MeshData load_stl_mesh(const std::filesystem::path& path, float uniform_scale)
{
    const auto optimized_path = preprocessed_mesh_path(path);
    if (std::filesystem::exists(optimized_path)) {
        return load_preprocessed_mesh(optimized_path, uniform_scale);
    }

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

        mesh.vertices.push_back(MeshVertex{vertex_a, normal, 0.5F, 0.0F});
        mesh.vertices.push_back(MeshVertex{vertex_b, normal, 0.5F, 0.0F});
        mesh.vertices.push_back(MeshVertex{vertex_c, normal, 0.5F, 0.0F});
    }

    return mesh;
}

} // namespace roberta::pendant
