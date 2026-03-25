#include "backdrop_renderer.hpp"

#include "platform_gl.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace roberta::pendant {
namespace {

#if defined(ROBERTA_PENDANT_USE_GLES2)
constexpr const char* k_vertex_shader = R"(
attribute vec2 a_position;

varying vec2 v_uv;

void main()
{
    v_uv = (a_position * 0.5) + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

constexpr const char* k_fragment_shader = R"(
precision mediump float;

uniform vec2 u_resolution;
uniform float u_time;

varying vec2 v_uv;

float hash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453123);
}

float beam_mask(vec2 uv, float center, float width, float slope)
{
    float distance = abs((uv.x - center) + ((uv.y - 0.75) * slope));
    return 1.0 - smoothstep(width, width + 0.08, distance);
}

void main()
{
    vec2 uv = v_uv;
    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 centered = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);

    vec3 sky = vec3(0.03, 0.025, 0.03);
    vec3 ember = vec3(0.24, 0.08, 0.04);
    vec3 rust = vec3(0.10, 0.06, 0.04);
    vec3 steel = vec3(0.05, 0.06, 0.07);

    float vertical_falloff = smoothstep(1.05, 0.12, uv.y);
    vec3 color = mix(vec3(0.01, 0.01, 0.015), sky, vertical_falloff);
    color += ember * smoothstep(0.88, 0.35, uv.y) * 0.18;
    color += rust * smoothstep(0.0, 0.42, uv.y) * 0.35;

    float haze = hash(floor(uv * vec2(160.0, 90.0)) + vec2(floor(u_time * 2.0), 0.0));
    color += vec3(0.015, 0.012, 0.01) * haze * smoothstep(1.0, 0.25, uv.y);

    float skylight = smoothstep(0.18, 0.0, length(vec2((uv.x - 0.72) * aspect, uv.y - 0.86)));
    color += vec3(0.34, 0.20, 0.11) * skylight * 0.9;

    float beam_a = beam_mask(uv, 0.62, 0.018, 0.55);
    float beam_b = beam_mask(uv, 0.71, 0.024, 0.38);
    float beam_c = beam_mask(uv, 0.80, 0.020, 0.20);
    float beam_strength = (beam_a * 0.9) + (beam_b * 1.1) + (beam_c * 0.75);
    beam_strength *= smoothstep(0.98, 0.38, uv.y);
    color += vec3(0.30, 0.13, 0.05) * beam_strength * 0.30;

    float truss = 1.0 - smoothstep(0.0, 0.010, abs(fract((uv.x * 5.8) + (uv.y * 0.35)) - 0.5));
    truss *= smoothstep(0.96, 0.76, uv.y) * 0.45;
    color *= 1.0 - (truss * 0.40);

    float columns = 0.0;
    columns += 1.0 - smoothstep(0.0, 0.030, abs(uv.x - 0.18));
    columns += 1.0 - smoothstep(0.0, 0.034, abs(uv.x - 0.47));
    columns += 1.0 - smoothstep(0.0, 0.026, abs(uv.x - 0.83));
    columns *= smoothstep(0.98, 0.08, uv.y);
    color = mix(color, steel * 0.45, min(columns * 0.32, 0.42));

    float floor_horizon = smoothstep(0.32, 0.72, uv.y);
    vec3 floor_color = mix(vec3(0.03, 0.028, 0.03), vec3(0.09, 0.05, 0.04), smoothstep(0.22, 1.0, uv.y));
    float floor_grid = (1.0 - smoothstep(0.0, 0.010, abs(fract((centered.x * 8.0) / max(uv.y + 0.22, 0.25)) - 0.5)))
                     + (1.0 - smoothstep(0.0, 0.010, abs(fract((uv.y * 14.0) + (centered.x * 0.5)) - 0.5)));
    floor_color -= vec3(0.025, 0.02, 0.018) * min(floor_grid * 0.10, 0.12);
    color = mix(color, floor_color, floor_horizon);

    float vignette = smoothstep(1.05, 0.22, length(centered * vec2(1.0, 1.35)));
    color *= vignette;

    gl_FragColor = vec4(color, 1.0);
}
)";
#else
constexpr const char* k_vertex_shader = R"(
#version 120

attribute vec2 a_position;

varying vec2 v_uv;

void main()
{
    v_uv = (a_position * 0.5) + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

constexpr const char* k_fragment_shader = R"(
#version 120

uniform vec2 u_resolution;
uniform float u_time;

varying vec2 v_uv;

float hash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453123);
}

float beam_mask(vec2 uv, float center, float width, float slope)
{
    float distance = abs((uv.x - center) + ((uv.y - 0.75) * slope));
    return 1.0 - smoothstep(width, width + 0.08, distance);
}

void main()
{
    vec2 uv = v_uv;
    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 centered = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);

    vec3 sky = vec3(0.03, 0.025, 0.03);
    vec3 ember = vec3(0.24, 0.08, 0.04);
    vec3 rust = vec3(0.10, 0.06, 0.04);
    vec3 steel = vec3(0.05, 0.06, 0.07);

    float vertical_falloff = smoothstep(1.05, 0.12, uv.y);
    vec3 color = mix(vec3(0.01, 0.01, 0.015), sky, vertical_falloff);
    color += ember * smoothstep(0.88, 0.35, uv.y) * 0.18;
    color += rust * smoothstep(0.0, 0.42, uv.y) * 0.35;

    float haze = hash(floor(uv * vec2(160.0, 90.0)) + vec2(floor(u_time * 2.0), 0.0));
    color += vec3(0.015, 0.012, 0.01) * haze * smoothstep(1.0, 0.25, uv.y);

    float skylight = smoothstep(0.18, 0.0, length(vec2((uv.x - 0.72) * aspect, uv.y - 0.86)));
    color += vec3(0.34, 0.20, 0.11) * skylight * 0.9;

    float beam_a = beam_mask(uv, 0.62, 0.018, 0.55);
    float beam_b = beam_mask(uv, 0.71, 0.024, 0.38);
    float beam_c = beam_mask(uv, 0.80, 0.020, 0.20);
    float beam_strength = (beam_a * 0.9) + (beam_b * 1.1) + (beam_c * 0.75);
    beam_strength *= smoothstep(0.98, 0.38, uv.y);
    color += vec3(0.30, 0.13, 0.05) * beam_strength * 0.30;

    float truss = 1.0 - smoothstep(0.0, 0.010, abs(fract((uv.x * 5.8) + (uv.y * 0.35)) - 0.5));
    truss *= smoothstep(0.96, 0.76, uv.y) * 0.45;
    color *= 1.0 - (truss * 0.40);

    float columns = 0.0;
    columns += 1.0 - smoothstep(0.0, 0.030, abs(uv.x - 0.18));
    columns += 1.0 - smoothstep(0.0, 0.034, abs(uv.x - 0.47));
    columns += 1.0 - smoothstep(0.0, 0.026, abs(uv.x - 0.83));
    columns *= smoothstep(0.98, 0.08, uv.y);
    color = mix(color, steel * 0.45, min(columns * 0.32, 0.42));

    float floor_horizon = smoothstep(0.32, 0.72, uv.y);
    vec3 floor_color = mix(vec3(0.03, 0.028, 0.03), vec3(0.09, 0.05, 0.04), smoothstep(0.22, 1.0, uv.y));
    float floor_grid = (1.0 - smoothstep(0.0, 0.010, abs(fract((centered.x * 8.0) / max(uv.y + 0.22, 0.25)) - 0.5)))
                     + (1.0 - smoothstep(0.0, 0.010, abs(fract((uv.y * 14.0) + (centered.x * 0.5)) - 0.5)));
    floor_color -= vec3(0.025, 0.02, 0.018) * min(floor_grid * 0.10, 0.12);
    color = mix(color, floor_color, floor_horizon);

    float vignette = smoothstep(1.05, 0.22, length(centered * vec2(1.0, 1.35)));
    color *= vignette;

    gl_FragColor = vec4(color, 1.0);
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
    throw std::runtime_error("backdrop shader compilation failed: " + log);
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
    throw std::runtime_error("backdrop program link failed: " + log);
}

} // namespace

BackdropRenderer::BackdropRenderer()
{
    constexpr std::array<float, 6> k_vertices{-1.0F, -1.0F, 3.0F, -1.0F, -1.0F, 3.0F};

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

    resolution_uniform_ = glGetUniformLocation(program_, "u_resolution");
    time_uniform_ = glGetUniformLocation(program_, "u_time");
    if (resolution_uniform_ < 0 || time_uniform_ < 0) {
        glDeleteProgram(program_);
        throw std::runtime_error("required backdrop uniform was not found");
    }

    glGenBuffers(1, &vertex_buffer_);
    if (vertex_buffer_ == 0U) {
        glDeleteProgram(program_);
        throw std::runtime_error("glGenBuffers failed for backdrop");
    }

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(k_vertices.size() * sizeof(float)), k_vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

BackdropRenderer::~BackdropRenderer()
{
    if (vertex_buffer_ != 0U) {
        auto buffer = static_cast<GLuint>(vertex_buffer_);
        glDeleteBuffers(1, &buffer);
    }

    if (program_ != 0U) {
        glDeleteProgram(program_);
    }
}

void BackdropRenderer::draw(int surface_width, int surface_height, float time_seconds) const
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(program_);
    glUniform2f(resolution_uniform_, static_cast<float>(std::max(surface_width, 1)),
                static_cast<float>(std::max(surface_height, 1)));
    glUniform1f(time_uniform_, time_seconds);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(float) * 2), nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
}

} // namespace roberta::pendant
