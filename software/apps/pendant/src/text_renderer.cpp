#include "text_renderer.hpp"

#include "platform_gl.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace roberta::pendant {
namespace {

constexpr int k_first_codepoint = 32;
constexpr int k_last_codepoint = 126;
constexpr int k_glyph_count = (k_last_codepoint - k_first_codepoint) + 1;
constexpr int k_initial_atlas_extent = 512;
constexpr int k_expanded_atlas_extent = 1024;

#if defined(ROBERTA_PENDANT_USE_GLES2)
constexpr const char* k_vertex_shader = R"(
attribute vec2 a_position;
attribute vec2 a_uv;

uniform vec2 u_viewport;

varying vec2 v_uv;

void main()
{
    vec2 normalized = vec2(
        (a_position.x / u_viewport.x) * 2.0 - 1.0,
        1.0 - (a_position.y / u_viewport.y) * 2.0
    );
    gl_Position = vec4(normalized, 0.0, 1.0);
    v_uv = a_uv;
}
)";

constexpr const char* k_fragment_shader = R"(
precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_color;

varying vec2 v_uv;

void main()
{
    float alpha = texture2D(u_texture, v_uv).a;
    gl_FragColor = vec4(u_color.rgb, u_color.a * alpha);
}
)";
#else
constexpr const char* k_vertex_shader = R"(
#version 120

attribute vec2 a_position;
attribute vec2 a_uv;

uniform vec2 u_viewport;

varying vec2 v_uv;

void main()
{
    vec2 normalized = vec2(
        (a_position.x / u_viewport.x) * 2.0 - 1.0,
        1.0 - (a_position.y / u_viewport.y) * 2.0
    );
    gl_Position = vec4(normalized, 0.0, 1.0);
    v_uv = a_uv;
}
)";

constexpr const char* k_fragment_shader = R"(
#version 120

uniform sampler2D u_texture;
uniform vec4 u_color;

varying vec2 v_uv;

void main()
{
    float alpha = texture2D(u_texture, v_uv).a;
    gl_FragColor = vec4(u_color.rgb, u_color.a * alpha);
}
)";
#endif

[[nodiscard]] std::uint32_t compile_shader(GLenum shader_type, const char* source)
{
    const auto shader = glCreateShader(shader_type);
    if (shader == 0U) {
        throw std::runtime_error("glCreateShader failed");
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("text shader compilation failed: " + log);
}

[[nodiscard]] std::uint32_t link_program(std::uint32_t vertex_shader, std::uint32_t fragment_shader)
{
    const auto program = glCreateProgram();
    if (program == 0U) {
        throw std::runtime_error("glCreateProgram failed");
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_uv");
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
    glGetProgramInfoLog(program, log_length, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("text program link failed: " + log);
}

[[nodiscard]] std::vector<std::uint8_t> load_file_bytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open font file: " + path.string());
    }

    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (size <= 0) {
        throw std::runtime_error("font file is empty: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("failed to read font file: " + path.string());
    }

    return bytes;
}

[[nodiscard]] std::filesystem::path find_font_path(std::string_view filename)
{
    const auto resolve = [&](const std::filesystem::path& root) -> std::filesystem::path {
        const auto candidate = root / "fonts" / std::filesystem::path(filename);
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        return {};
    };

    if (const char* asset_root = std::getenv("ROBERTA_PENDANT_ASSET_ROOT"); asset_root != nullptr) {
        if (auto resolved = resolve(asset_root); !resolved.empty()) {
            return resolved;
        }
    }

    const auto current_directory = std::filesystem::current_path();
    if (auto resolved = resolve(current_directory / "assets"); !resolved.empty()) {
        return resolved;
    }
    if (auto resolved = resolve(current_directory / "software" / "apps" / "pendant" / "assets"); !resolved.empty()) {
        return resolved;
    }

    if (char* base_path = SDL_GetBasePath(); base_path != nullptr) {
        const std::filesystem::path executable_root(base_path);
        SDL_free(base_path);

        if (auto resolved = resolve(executable_root / "assets"); !resolved.empty()) {
            return resolved;
        }
        if (auto resolved = resolve(executable_root / ".." / "assets"); !resolved.empty()) {
            return resolved;
        }
    }

    throw std::runtime_error(
        "could not find pendant font assets; copy the assets directory next to the binary or set ROBERTA_PENDANT_ASSET_ROOT");
}

[[nodiscard]] std::filesystem::path font_path_for_face(FontFace face)
{
    switch (face) {
    case FontFace::sans:
        return find_font_path("Geist-Regular.ttf");
    case FontFace::mono:
        return find_font_path("GeistMono-Regular.ttf");
    }

    throw std::runtime_error("unsupported font face");
}

[[nodiscard]] std::uint32_t atlas_key(FontFace face, int pixel_height)
{
    return (static_cast<std::uint32_t>(face) << 16U) | static_cast<std::uint32_t>(pixel_height);
}

} // namespace

struct TextRenderer::FontAtlas final {
    std::uint32_t texture{0};
    int width{0};
    int height{0};
    int pixel_height{0};
    float line_height{0.0F};
    std::array<stbtt_bakedchar, k_glyph_count> glyphs{};
};

struct TextRenderer::Storage final {
    struct Vertex final {
        float x;
        float y;
        float u;
        float v;
    };

    std::unordered_map<std::uint32_t, TextRenderer::FontAtlas> atlases;
    std::vector<Vertex> vertices;
};

TextRenderer::TextRenderer()
    : storage_(new Storage())
{
    const auto vertex_shader = compile_shader(GL_VERTEX_SHADER, k_vertex_shader);
    const auto fragment_shader = compile_shader(GL_FRAGMENT_SHADER, k_fragment_shader);

    try {
        program_ = link_program(vertex_shader, fragment_shader);
    } catch (...) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        throw;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    vertex_buffer_ = buffer;
    if (vertex_buffer_ == 0U) {
        glDeleteProgram(program_);
        throw std::runtime_error("glGenBuffers failed for text renderer");
    }

    viewport_uniform_ = glGetUniformLocation(program_, "u_viewport");
    color_uniform_ = glGetUniformLocation(program_, "u_color");
    texture_uniform_ = glGetUniformLocation(program_, "u_texture");
    if (viewport_uniform_ < 0 || color_uniform_ < 0 || texture_uniform_ < 0) {
        glDeleteBuffers(1, &buffer);
        glDeleteProgram(program_);
        throw std::runtime_error("required text renderer uniform was not found");
    }

    storage_->vertices.reserve(512);
}

TextRenderer::~TextRenderer()
{
    for (auto& [_, atlas] : storage_->atlases) {
        if (atlas.texture != 0U) {
            auto texture = static_cast<GLuint>(atlas.texture);
            glDeleteTextures(1, &texture);
        }
    }
    delete storage_;

    if (vertex_buffer_ != 0U) {
        auto buffer = static_cast<GLuint>(vertex_buffer_);
        glDeleteBuffers(1, &buffer);
    }
    if (program_ != 0U) {
        glDeleteProgram(program_);
    }
}

void TextRenderer::set_viewport(int width, int height)
{
    viewport_width_ = std::max(width, 1);
    viewport_height_ = std::max(height, 1);
}

TextRenderer::FontAtlas& TextRenderer::atlas_for_style(const TextStyle& style)
{
    const auto requested_size = std::max(8, static_cast<int>(std::lround(style.pixel_height)));
    const auto key = atlas_key(style.font, requested_size);

    if (auto existing = storage_->atlases.find(key); existing != storage_->atlases.end()) {
        return existing->second;
    }

    const auto font_path = font_path_for_face(style.font);
    const auto font_bytes = load_file_bytes(font_path);
    FontAtlas atlas{};
    atlas.pixel_height = requested_size;
    atlas.line_height = static_cast<float>(requested_size) * 1.30F;

    std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(k_initial_atlas_extent * k_initial_atlas_extent));
    int bake_result = stbtt_BakeFontBitmap(font_bytes.data(), 0, static_cast<float>(requested_size), bitmap.data(),
                                           k_initial_atlas_extent, k_initial_atlas_extent, k_first_codepoint,
                                           k_glyph_count, atlas.glyphs.data());

    atlas.width = k_initial_atlas_extent;
    atlas.height = k_initial_atlas_extent;
    if (bake_result <= 0) {
        atlas.width = k_expanded_atlas_extent;
        atlas.height = k_expanded_atlas_extent;
        bitmap.assign(static_cast<std::size_t>(atlas.width * atlas.height), 0);
        bake_result = stbtt_BakeFontBitmap(font_bytes.data(), 0, static_cast<float>(requested_size), bitmap.data(),
                                           atlas.width, atlas.height, k_first_codepoint, k_glyph_count,
                                           atlas.glyphs.data());
        if (bake_result <= 0) {
            throw std::runtime_error("font atlas bake failed for " + font_path.string());
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0U) {
        throw std::runtime_error("glGenTextures failed for text renderer");
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas.width, atlas.height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    atlas.texture = texture;
    return storage_->atlases.emplace(key, std::move(atlas)).first->second;
}

void TextRenderer::draw_text(std::string_view text, float x, float y, const TextStyle& style)
{
    auto& atlas = atlas_for_style(style);
    auto& vertices = storage_->vertices;
    vertices.clear();

    auto pen_x = x;
    auto pen_y = y + static_cast<float>(atlas.pixel_height);
    const auto line_advance = atlas.line_height * std::max(style.line_spacing, 1.0F);

    for (const char ch : text) {
        if (ch == '\n') {
            pen_x = x;
            pen_y += line_advance;
            continue;
        }

        auto codepoint = static_cast<unsigned char>(ch);
        if (codepoint < k_first_codepoint || codepoint > k_last_codepoint) {
            codepoint = static_cast<unsigned char>('?');
        }

        stbtt_aligned_quad quad{};
        stbtt_GetBakedQuad(atlas.glyphs.data(), atlas.width, atlas.height, codepoint - k_first_codepoint, &pen_x, &pen_y,
                           &quad, 1);

        vertices.push_back(Storage::Vertex{quad.x0, quad.y0, quad.s0, quad.t0});
        vertices.push_back(Storage::Vertex{quad.x1, quad.y0, quad.s1, quad.t0});
        vertices.push_back(Storage::Vertex{quad.x1, quad.y1, quad.s1, quad.t1});
        vertices.push_back(Storage::Vertex{quad.x0, quad.y0, quad.s0, quad.t0});
        vertices.push_back(Storage::Vertex{quad.x1, quad.y1, quad.s1, quad.t1});
        vertices.push_back(Storage::Vertex{quad.x0, quad.y1, quad.s0, quad.t1});
    }

    if (vertices.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glUniform2f(viewport_uniform_, static_cast<float>(viewport_width_), static_cast<float>(viewport_height_));
    glUniform4f(color_uniform_, style.color.red, style.color.green, style.color.blue, style.color.alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas.texture);
    glUniform1i(texture_uniform_, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Storage::Vertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Storage::Vertex),
                          reinterpret_cast<const void*>(offsetof(Storage::Vertex, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Storage::Vertex),
                          reinterpret_cast<const void*>(offsetof(Storage::Vertex, u)));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace roberta::pendant
