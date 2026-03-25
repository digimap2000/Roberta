#include "line_renderer.hpp"

#include "platform_gl.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace roberta::pendant {
namespace {

#if defined(ROBERTA_PENDANT_USE_GLES2)
constexpr const char* k_vertex_shader = R"(
attribute vec3 a_position;
attribute vec4 a_color;

uniform mat4 u_view_projection;

varying vec4 v_color;

void main()
{
    gl_Position = u_view_projection * vec4(a_position, 1.0);
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
#else
constexpr const char* k_vertex_shader = R"(
#version 120

attribute vec3 a_position;
attribute vec4 a_color;

uniform mat4 u_view_projection;

varying vec4 v_color;

void main()
{
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    v_color = a_color;
}
)";

constexpr const char* k_fragment_shader = R"(
#version 120

varying vec4 v_color;

void main()
{
    gl_FragColor = v_color;
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

LineRenderer::LineRenderer()
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

    view_projection_uniform_ = glGetUniformLocation(program_, "u_view_projection");
    if (view_projection_uniform_ < 0) {
        glDeleteProgram(program_);
        throw std::runtime_error("required line renderer uniform was not found");
    }

    glGenBuffers(1, &vertex_buffer_);
    if (vertex_buffer_ == 0U) {
        glDeleteProgram(program_);
        throw std::runtime_error("glGenBuffers failed for line renderer");
    }
}

LineRenderer::~LineRenderer()
{
    if (vertex_buffer_ != 0U) {
        glDeleteBuffers(1, &vertex_buffer_);
    }

    if (program_ != 0U) {
        glDeleteProgram(program_);
    }
}

void LineRenderer::draw_lines(const std::vector<Vertex>& vertices, const Mat4& view_projection, bool depth_test)
{
    if (vertices.empty()) {
        return;
    }

    glUseProgram(program_);
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, view_projection.ptr());

    if (depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(),
                 GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, color)));
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDepthMask(GL_TRUE);
}

} // namespace roberta::pendant
