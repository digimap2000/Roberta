#pragma once

#include "gles2_renderer.hpp"

#include <cstdint>
#include <string_view>

namespace roberta::pendant {

enum class FontFace : std::uint8_t {
    sans,
    mono,
};

struct TextStyle final {
    FontFace font{FontFace::sans};
    float pixel_height{18.0F};
    float line_spacing{1.2F};
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
};

class TextRenderer final {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    TextRenderer(TextRenderer&&) = delete;
    TextRenderer& operator=(TextRenderer&&) = delete;

    void set_viewport(int width, int height);
    void draw_text(std::string_view text, float x, float y, const TextStyle& style);

private:
    struct FontAtlas;
    struct Storage;
    [[nodiscard]] FontAtlas& atlas_for_style(const TextStyle& style);

    std::uint32_t program_{0};
    std::uint32_t vertex_buffer_{0};
    int viewport_width_{1};
    int viewport_height_{1};
    int viewport_uniform_{-1};
    int color_uniform_{-1};
    int texture_uniform_{-1};
    Storage* storage_{nullptr};
};

} // namespace roberta::pendant
