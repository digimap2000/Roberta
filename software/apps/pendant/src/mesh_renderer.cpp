#include "mesh_renderer.hpp"

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
attribute vec3 a_normal;

uniform mat4 u_view_projection;
uniform mat4 u_model;

varying vec3 v_normal;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    gl_Position = u_view_projection * world_position;
    v_normal = mat3(u_model) * a_normal;
}
)";

constexpr const char* k_fragment_shader = R"(
precision mediump float;

uniform vec4 u_color;
uniform vec3 u_light_direction;

varying vec3 v_normal;

void main()
{
    vec3 normal = normalize(v_normal);
    float diffuse = max(dot(normal, normalize(-u_light_direction)), 0.0);
    float ambient = 0.25;
    float rim = pow(1.0 - max(normal.z, 0.0), 2.0) * 0.15;
    vec3 lit = u_color.rgb * (ambient + (diffuse * 0.75) + rim);
    gl_FragColor = vec4(lit, u_color.a);
}
)";
#else
constexpr const char* k_vertex_shader = R"(
#version 120

attribute vec3 a_position;
attribute vec3 a_normal;

uniform mat4 u_view_projection;
uniform mat4 u_model;

varying vec3 v_normal;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    gl_Position = u_view_projection * world_position;
    v_normal = mat3(u_model) * a_normal;
}
)";

constexpr const char* k_fragment_shader = R"(
#version 120

uniform vec4 u_color;
uniform vec3 u_light_direction;

varying vec3 v_normal;

void main()
{
    vec3 normal = normalize(v_normal);
    float diffuse = max(dot(normal, normalize(-u_light_direction)), 0.0);
    float ambient = 0.25;
    float rim = pow(1.0 - max(normal.z, 0.0), 2.0) * 0.15;
    vec3 lit = u_color.rgb * (ambient + (diffuse * 0.75) + rim);
    gl_FragColor = vec4(lit, u_color.a);
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
    glBindAttribLocation(program, 1, "a_normal");
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

MeshRenderer::MeshRenderer()
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
    model_uniform_ = glGetUniformLocation(program_, "u_model");
    color_uniform_ = glGetUniformLocation(program_, "u_color");
    light_direction_uniform_ = glGetUniformLocation(program_, "u_light_direction");

    if (view_projection_uniform_ < 0 || model_uniform_ < 0 || color_uniform_ < 0 || light_direction_uniform_ < 0) {
        glDeleteProgram(program_);
        throw std::runtime_error("required shader uniform was not found");
    }
}

MeshRenderer::~MeshRenderer()
{
    if (program_ != 0U) {
        glDeleteProgram(program_);
    }
}

void MeshRenderer::set_viewport(int width, int height)
{
    viewport_width_ = std::max(width, 1);
    viewport_height_ = std::max(height, 1);
    glViewport(0, 0, viewport_width_, viewport_height_);
}

void MeshRenderer::begin_frame(const Vec4& clear_color)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

MeshRenderer::GpuMesh MeshRenderer::upload_mesh(const MeshData& mesh)
{
    if (mesh.vertices.empty()) {
        throw std::runtime_error("mesh contains no vertices");
    }

    GLuint vertex_buffer = 0;
    glGenBuffers(1, &vertex_buffer);
    if (vertex_buffer == 0U) {
        throw std::runtime_error("glGenBuffers failed for mesh upload");
    }

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(MeshVertex)), mesh.vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return GpuMesh{
        .vertex_buffer = vertex_buffer,
        .vertex_count = static_cast<int>(mesh.vertices.size()),
    };
}

void MeshRenderer::draw_mesh(const GpuMesh& mesh, const Mat4& model, const Mat4& view_projection, const Vec4& color)
{
    glUseProgram(program_);
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, view_projection.ptr());
    glUniformMatrix4fv(model_uniform_, 1, GL_FALSE, model.ptr());
    glUniform4f(color_uniform_, color.x, color.y, color.z, color.w);
    glUniform3f(light_direction_uniform_, -0.5F, 0.8F, 0.6F);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));

    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace roberta::pendant
