#include "gles2_renderer.hpp"

#include "platform_gl.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace roberta::pendant {
namespace {

constexpr const char* k_vertex_shader = R"(
attribute vec2 a_position;
attribute vec4 a_color;

uniform vec2 u_viewport;

varying vec4 v_color;

void main()
{
    vec2 normalized = vec2(
        (a_position.x / u_viewport.x) * 2.0 - 1.0,
        1.0 - (a_position.y / u_viewport.y) * 2.0
    );
    gl_Position = vec4(normalized, 0.0, 1.0);
    v_color = a_color;
}
)";

constexpr const char* k_fragment_shader = R"(
precision mediump float;

varying vec4 v_color;

void main()
{
    gl_FragColor = v_color;
}
)";

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
    throw std::runtime_error("shader compilation failed: " + log);
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
    glBindAttribLocation(program, 1, "a_color");
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
    throw std::runtime_error("program link failed: " + log);
}

} // namespace

Color lerp(const Color& from, const Color& to, float amount)
{
    const auto t = clamp01(amount);
    return Color{
        .red = from.red + (to.red - from.red) * t,
        .green = from.green + (to.green - from.green) * t,
        .blue = from.blue + (to.blue - from.blue) * t,
        .alpha = from.alpha + (to.alpha - from.alpha) * t,
    };
}

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

Gles2Renderer::Gles2Renderer()
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
        throw std::runtime_error("glGenBuffers failed");
    }

    viewport_uniform_ = glGetUniformLocation(program_, "u_viewport");
    if (viewport_uniform_ < 0) {
        glDeleteBuffers(1, &buffer);
        glDeleteProgram(program_);
        throw std::runtime_error("u_viewport uniform not found");
    }

    vertices_.reserve(512);
}

Gles2Renderer::~Gles2Renderer()
{
    if (vertex_buffer_ != 0U) {
        const auto buffer = static_cast<GLuint>(vertex_buffer_);
        glDeleteBuffers(1, &buffer);
    }
    if (program_ != 0U) {
        glDeleteProgram(program_);
    }
}

void Gles2Renderer::set_viewport(int width, int height)
{
    viewport_width_ = std::max(width, 1);
    viewport_height_ = std::max(height, 1);
    glViewport(0, 0, viewport_width_, viewport_height_);
}

void Gles2Renderer::begin_frame(const Color& clear_color)
{
    vertices_.clear();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(clear_color.red, clear_color.green, clear_color.blue, clear_color.alpha);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Gles2Renderer::fill_rect(const Rect& rect, const Color& color)
{
    if (rect.width <= 0.0F || rect.height <= 0.0F) {
        return;
    }

    const auto x0 = rect.x;
    const auto x1 = rect.x + rect.width;
    const auto y0 = rect.y;
    const auto y1 = rect.y + rect.height;

    const std::array<Vertex, 6> quad{{
        Vertex{x0, y0, color.red, color.green, color.blue, color.alpha},
        Vertex{x1, y0, color.red, color.green, color.blue, color.alpha},
        Vertex{x1, y1, color.red, color.green, color.blue, color.alpha},
        Vertex{x0, y0, color.red, color.green, color.blue, color.alpha},
        Vertex{x1, y1, color.red, color.green, color.blue, color.alpha},
        Vertex{x0, y1, color.red, color.green, color.blue, color.alpha},
    }};

    vertices_.insert(vertices_.end(), quad.begin(), quad.end());
}

void Gles2Renderer::stroke_rect(const Rect& rect, float thickness, const Color& color)
{
    const auto inset = std::max(0.0F, thickness);
    if (rect.width <= 0.0F || rect.height <= 0.0F || inset <= 0.0F) {
        return;
    }

    const auto clamped_thickness = std::min(inset, std::min(rect.width, rect.height) * 0.5F);

    fill_rect(Rect{rect.x, rect.y, rect.width, clamped_thickness}, color);
    fill_rect(Rect{rect.x, rect.y + rect.height - clamped_thickness, rect.width, clamped_thickness}, color);
    fill_rect(Rect{rect.x, rect.y + clamped_thickness, clamped_thickness, rect.height - (clamped_thickness * 2.0F)}, color);
    fill_rect(Rect{rect.x + rect.width - clamped_thickness, rect.y + clamped_thickness, clamped_thickness,
                   rect.height - (clamped_thickness * 2.0F)}, color);
}

void Gles2Renderer::flush()
{
    if (vertices_.empty()) {
        return;
    }

    glUseProgram(program_);
    glUniform2f(viewport_uniform_, static_cast<float>(viewport_width_), static_cast<float>(viewport_height_));

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(),
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, red)));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace roberta::pendant
