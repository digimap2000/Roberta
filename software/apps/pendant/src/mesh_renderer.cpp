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
attribute vec2 a_surface_data;

uniform mat4 u_view_projection;
uniform mat4 u_model;

varying vec3 v_normal;
varying vec3 v_world_position;
varying vec2 v_surface_data;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    gl_Position = u_view_projection * world_position;
    v_normal = mat3(u_model) * a_normal;
    v_world_position = world_position.xyz;
    v_surface_data = a_surface_data;
}
)";

constexpr const char* k_fragment_shader = R"(
precision mediump float;

uniform vec4 u_base_color;
uniform vec4 u_detail_color;
uniform vec4 u_material_response;
uniform vec3 u_light_direction;
uniform vec3 u_camera_position;
uniform vec2 u_attention;

varying vec3 v_normal;
varying vec3 v_world_position;
varying vec2 v_surface_data;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 key_direction = normalize(-u_light_direction);
    vec3 view_direction = normalize(u_camera_position - v_world_position);
    vec3 fill_direction = normalize(vec3(-key_direction.y * 0.55, key_direction.x * 0.55, 0.45));
    vec3 half_vector = normalize(key_direction + view_direction);

    float roughness = clamp(u_material_response.x, 0.08, 1.0);
    float specular_strength = u_material_response.y;
    float variation_strength = u_material_response.z;
    float edge_strength = u_material_response.w;
    float focus = clamp(u_attention.x, 0.0, 1.0);
    float mute = clamp(u_attention.y, 0.0, 1.0);
    roughness = mix(roughness, clamp(roughness + 0.24, 0.08, 1.0), mute);
    roughness = mix(roughness, max(0.08, roughness * 0.76), focus);
    specular_strength *= mix(1.0, 0.52, mute) * mix(1.0, 1.35, focus);
    variation_strength *= mix(1.0, 0.58, mute);
    edge_strength *= mix(1.0, 0.70, mute) * mix(1.0, 1.45, focus);
    float surface_variation = ((v_surface_data.x * 2.0) - 1.0) * variation_strength;
    float edge_mask = clamp(v_surface_data.y * edge_strength, 0.0, 1.0);

    vec3 albedo = clamp(u_base_color.rgb * (1.0 + surface_variation), 0.0, 1.0);
    albedo = mix(albedo, u_detail_color.rgb, edge_mask * 0.22);
    float neutral = dot(albedo, vec3(0.299, 0.587, 0.114));
    albedo = mix(albedo, vec3(neutral * 0.76), mute * 0.58);

    float key = max(dot(normal, key_direction), 0.0);
    float fill = max(dot(normal, fill_direction), 0.0);
    float hemisphere = mix(0.08, 0.22, clamp((normal.z * 0.5) + 0.5, 0.0, 1.0));
    float rim = pow(1.0 - max(dot(normal, view_direction), 0.0), mix(4.4, 2.8, roughness))
                * (0.08 + edge_mask + (0.20 * (1.0 - key)));
    float specular = pow(max(dot(normal, half_vector), 0.0), mix(48.0, 10.0, roughness))
                     * specular_strength * (0.10 + (0.90 * key));
    float focus_halo = pow(1.0 - max(dot(normal, view_direction), 0.0), 3.1)
                       * focus * (0.16 + (0.24 * edge_mask) + (0.12 * (1.0 - key)));

    vec3 base = albedo * (hemisphere + (key * 0.78) + (fill * 0.14)) * mix(1.0, 0.84, mute) * mix(1.0, 1.10, focus);
    vec3 lit = base + (u_detail_color.rgb * ((fill * 0.05) + (rim * 0.22) + (edge_mask * 0.10)))
               + (mix(vec3(0.18, 0.19, 0.20), u_detail_color.rgb, 0.45) * specular)
               + (mix(u_detail_color.rgb, vec3(1.0), 0.32) * focus_halo);
    lit = min(lit, vec3(1.0));
    gl_FragColor = vec4(lit, u_base_color.a);
}
)";
#else
constexpr const char* k_vertex_shader = R"(
#version 120

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_surface_data;

uniform mat4 u_view_projection;
uniform mat4 u_model;

varying vec3 v_normal;
varying vec3 v_world_position;
varying vec2 v_surface_data;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    gl_Position = u_view_projection * world_position;
    v_normal = mat3(u_model) * a_normal;
    v_world_position = world_position.xyz;
    v_surface_data = a_surface_data;
}
)";

constexpr const char* k_fragment_shader = R"(
#version 120

uniform vec4 u_base_color;
uniform vec4 u_detail_color;
uniform vec4 u_material_response;
uniform vec3 u_light_direction;
uniform vec3 u_camera_position;
uniform vec2 u_attention;

varying vec3 v_normal;
varying vec3 v_world_position;
varying vec2 v_surface_data;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 key_direction = normalize(-u_light_direction);
    vec3 view_direction = normalize(u_camera_position - v_world_position);
    vec3 fill_direction = normalize(vec3(-key_direction.y * 0.55, key_direction.x * 0.55, 0.45));
    vec3 half_vector = normalize(key_direction + view_direction);

    float roughness = clamp(u_material_response.x, 0.08, 1.0);
    float specular_strength = u_material_response.y;
    float variation_strength = u_material_response.z;
    float edge_strength = u_material_response.w;
    float focus = clamp(u_attention.x, 0.0, 1.0);
    float mute = clamp(u_attention.y, 0.0, 1.0);
    roughness = mix(roughness, clamp(roughness + 0.24, 0.08, 1.0), mute);
    roughness = mix(roughness, max(0.08, roughness * 0.76), focus);
    specular_strength *= mix(1.0, 0.52, mute) * mix(1.0, 1.35, focus);
    variation_strength *= mix(1.0, 0.58, mute);
    edge_strength *= mix(1.0, 0.70, mute) * mix(1.0, 1.45, focus);
    float surface_variation = ((v_surface_data.x * 2.0) - 1.0) * variation_strength;
    float edge_mask = clamp(v_surface_data.y * edge_strength, 0.0, 1.0);

    vec3 albedo = clamp(u_base_color.rgb * (1.0 + surface_variation), 0.0, 1.0);
    albedo = mix(albedo, u_detail_color.rgb, edge_mask * 0.22);
    float neutral = dot(albedo, vec3(0.299, 0.587, 0.114));
    albedo = mix(albedo, vec3(neutral * 0.76), mute * 0.58);

    float key = max(dot(normal, key_direction), 0.0);
    float fill = max(dot(normal, fill_direction), 0.0);
    float hemisphere = mix(0.08, 0.22, clamp((normal.z * 0.5) + 0.5, 0.0, 1.0));
    float rim = pow(1.0 - max(dot(normal, view_direction), 0.0), mix(4.4, 2.8, roughness))
                * (0.08 + edge_mask + (0.20 * (1.0 - key)));
    float specular = pow(max(dot(normal, half_vector), 0.0), mix(48.0, 10.0, roughness))
                     * specular_strength * (0.10 + (0.90 * key));
    float focus_halo = pow(1.0 - max(dot(normal, view_direction), 0.0), 3.1)
                       * focus * (0.16 + (0.24 * edge_mask) + (0.12 * (1.0 - key)));

    vec3 base = albedo * (hemisphere + (key * 0.78) + (fill * 0.14)) * mix(1.0, 0.84, mute) * mix(1.0, 1.10, focus);
    vec3 lit = base + (u_detail_color.rgb * ((fill * 0.05) + (rim * 0.22) + (edge_mask * 0.10)))
               + (mix(vec3(0.18, 0.19, 0.20), u_detail_color.rgb, 0.45) * specular)
               + (mix(u_detail_color.rgb, vec3(1.0), 0.32) * focus_halo);
    lit = min(lit, vec3(1.0));
    gl_FragColor = vec4(lit, u_base_color.a);
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
    glBindAttribLocation(program, 2, "a_surface_data");
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
    base_color_uniform_ = glGetUniformLocation(program_, "u_base_color");
    detail_color_uniform_ = glGetUniformLocation(program_, "u_detail_color");
    material_response_uniform_ = glGetUniformLocation(program_, "u_material_response");
    light_direction_uniform_ = glGetUniformLocation(program_, "u_light_direction");
    camera_position_uniform_ = glGetUniformLocation(program_, "u_camera_position");
    attention_uniform_ = glGetUniformLocation(program_, "u_attention");

    if (view_projection_uniform_ < 0 || model_uniform_ < 0 || base_color_uniform_ < 0 || detail_color_uniform_ < 0
        || material_response_uniform_ < 0 || light_direction_uniform_ < 0 || camera_position_uniform_ < 0
        || attention_uniform_ < 0) {
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

void MeshRenderer::draw_mesh(const GpuMesh& mesh, const Mat4& model, const Mat4& view_projection,
                            const Material& material, const Vec3& light_direction, const Vec3& camera_position,
                            float focus_amount, float mute_amount)
{
    const auto normalized_light_direction = normalize(light_direction);

    glUseProgram(program_);
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, view_projection.ptr());
    glUniformMatrix4fv(model_uniform_, 1, GL_FALSE, model.ptr());
    glUniform4f(base_color_uniform_, material.base_color.x, material.base_color.y, material.base_color.z,
                material.base_color.w);
    glUniform4f(detail_color_uniform_, material.detail_color.x, material.detail_color.y, material.detail_color.z,
                material.detail_color.w);
    glUniform4f(material_response_uniform_, material.response.x, material.response.y, material.response.z,
                material.response.w);
    glUniform3f(light_direction_uniform_, normalized_light_direction.x, normalized_light_direction.y,
                normalized_light_direction.z);
    glUniform3f(camera_position_uniform_, camera_position.x, camera_position.y, camera_position.z);
    glUniform2f(attention_uniform_, focus_amount, mute_amount);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, surface_variation)));

    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace roberta::pendant
