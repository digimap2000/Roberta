#pragma once

#include <cstdint>

namespace roberta::pendant {

class BackdropRenderer final {
public:
    BackdropRenderer();
    ~BackdropRenderer();

    BackdropRenderer(const BackdropRenderer&) = delete;
    BackdropRenderer& operator=(const BackdropRenderer&) = delete;

    void draw(int surface_width, int surface_height, float time_seconds) const;

private:
    std::uint32_t program_{0};
    std::uint32_t vertex_buffer_{0};
    int resolution_uniform_{-1};
    int time_uniform_{-1};
};

} // namespace roberta::pendant
