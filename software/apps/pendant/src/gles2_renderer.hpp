#pragma once

#include <cstdint>
#include <vector>

namespace roberta::pendant {

struct Color final {
    float red;
    float green;
    float blue;
    float alpha;
};

struct Rect final {
    float x;
    float y;
    float width;
    float height;
};

class Gles2Renderer final {
public:
    Gles2Renderer();
    ~Gles2Renderer();

    Gles2Renderer(const Gles2Renderer&) = delete;
    Gles2Renderer& operator=(const Gles2Renderer&) = delete;

    Gles2Renderer(Gles2Renderer&&) = delete;
    Gles2Renderer& operator=(Gles2Renderer&&) = delete;

    void set_viewport(int width, int height);
    void begin_frame(const Color& clear_color);
    void fill_rect(const Rect& rect, const Color& color);
    void stroke_rect(const Rect& rect, float thickness, const Color& color);
    void flush();

private:
    struct Vertex final {
        float x;
        float y;
        float red;
        float green;
        float blue;
        float alpha;
    };

    std::uint32_t program_{0};
    std::uint32_t vertex_buffer_{0};
    int viewport_width_{1};
    int viewport_height_{1};
    int viewport_uniform_{-1};
    std::vector<Vertex> vertices_;
};

[[nodiscard]] Color lerp(const Color& from, const Color& to, float amount);
[[nodiscard]] float clamp01(float value);

} // namespace roberta::pendant
