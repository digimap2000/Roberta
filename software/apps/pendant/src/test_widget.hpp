#pragma once

namespace roberta::pendant {

class Gles2Renderer;

class TestWidget final {
public:
    void nudge_target(float delta);
    void nudge_response(float delta);
    void toggle_motion();
    void toggle_palette();
    void reset();

    void update(float delta_seconds);
    void draw(Gles2Renderer& renderer, int surface_width, int surface_height) const;

private:
    float value_{0.28F};
    float target_{0.72F};
    float response_{3.0F};
    float phase_{0.0F};
    bool motion_enabled_{true};
    bool warm_palette_{false};
};

} // namespace roberta::pendant
