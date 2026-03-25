#include "test_widget.hpp"

#include "gles2_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace roberta::pendant {
namespace {

[[nodiscard]] Rect inset(const Rect& rect, float amount)
{
    return Rect{
        .x = rect.x + amount,
        .y = rect.y + amount,
        .width = std::max(0.0F, rect.width - (amount * 2.0F)),
        .height = std::max(0.0F, rect.height - (amount * 2.0F)),
    };
}

} // namespace

void TestWidget::nudge_target(float delta)
{
    target_ = clamp01(target_ + delta);
}

void TestWidget::nudge_response(float delta)
{
    response_ = std::clamp(response_ + delta, 0.5F, 8.0F);
}

void TestWidget::toggle_motion()
{
    motion_enabled_ = !motion_enabled_;
}

void TestWidget::toggle_palette()
{
    warm_palette_ = !warm_palette_;
}

void TestWidget::reset()
{
    value_ = 0.28F;
    target_ = 0.72F;
    response_ = 3.0F;
    phase_ = 0.0F;
    motion_enabled_ = true;
    warm_palette_ = false;
}

void TestWidget::update(float delta_seconds)
{
    const auto dt = std::max(0.0F, delta_seconds);
    phase_ += dt;

    if (!motion_enabled_) {
        return;
    }

    const auto blend = std::min(dt * response_, 1.0F);
    value_ += (target_ - value_) * blend;
}

void TestWidget::draw(Gles2Renderer& renderer, int surface_width, int surface_height) const
{
    const auto width = static_cast<float>(std::max(surface_width, 1));
    const auto height = static_cast<float>(std::max(surface_height, 1));
    const auto margin = std::min(width, height) * 0.08F;

    const Rect card{
        .x = margin,
        .y = margin,
        .width = width - (margin * 2.0F),
        .height = height - (margin * 2.0F),
    };

    const auto pulse = 0.5F + (0.5F * std::sin(phase_ * (warm_palette_ ? 3.4F : 2.1F)));
    const auto base = warm_palette_ ? Color{0.94F, 0.43F, 0.20F, 1.0F} : Color{0.10F, 0.74F, 0.68F, 1.0F};
    const auto accent = lerp(base, Color{0.98F, 0.96F, 0.88F, 1.0F}, 0.18F + (pulse * 0.20F));

    renderer.fill_rect(card, Color{0.08F, 0.10F, 0.13F, 1.0F});
    renderer.stroke_rect(card, 3.0F, Color{0.22F, 0.28F, 0.32F, 1.0F});

    const auto chrome = inset(card, 28.0F);
    const Rect header{
        .x = chrome.x,
        .y = chrome.y,
        .width = chrome.width,
        .height = 18.0F,
    };
    renderer.fill_rect(header, lerp(accent, Color{1.0F, 1.0F, 1.0F, 1.0F}, 0.10F));

    const Rect viewport{
        .x = chrome.x,
        .y = chrome.y + 48.0F,
        .width = chrome.width,
        .height = chrome.height - 112.0F,
    };
    renderer.fill_rect(viewport, Color{0.12F, 0.15F, 0.19F, 1.0F});

    const Rect preview = inset(Rect{viewport.x, viewport.y, viewport.width * 0.26F, viewport.height}, 18.0F);
    renderer.stroke_rect(preview, 2.0F, Color{0.22F, 0.28F, 0.32F, 1.0F});

    const auto preview_inner = inset(preview, 10.0F);
    const auto preview_fill_height = preview_inner.height * std::clamp(value_, 0.0F, 1.0F);
    renderer.fill_rect(
        Rect{preview_inner.x, preview_inner.y + preview_inner.height - preview_fill_height, preview_inner.width,
             std::max(0.0F, preview_fill_height)},
        Color{accent.red, accent.green, accent.blue, 0.92F});

    const Rect track{
        .x = viewport.x + (viewport.width * 0.34F),
        .y = viewport.y + (viewport.height * 0.44F),
        .width = viewport.width * 0.56F,
        .height = 44.0F,
    };
    renderer.fill_rect(track, Color{0.16F, 0.19F, 0.23F, 1.0F});
    renderer.stroke_rect(track, 2.0F, Color{0.26F, 0.31F, 0.36F, 1.0F});

    const auto current_width = std::max(16.0F, (track.width - 12.0F) * std::clamp(value_, 0.0F, 1.0F));
    renderer.fill_rect(Rect{track.x + 6.0F, track.y + 6.0F, current_width, track.height - 12.0F},
                       Color{accent.red, accent.green, accent.blue, 0.95F});

    constexpr int tick_count = 6;
    for (int tick = 1; tick < tick_count; ++tick) {
        const auto x = track.x + (track.width * (static_cast<float>(tick) / static_cast<float>(tick_count)));
        renderer.fill_rect(Rect{x - 1.0F, track.y + 8.0F, 2.0F, track.height - 16.0F}, Color{1.0F, 1.0F, 1.0F, 0.08F});
    }

    const auto target_x = track.x + (track.width * std::clamp(target_, 0.0F, 1.0F));
    renderer.fill_rect(Rect{target_x - 3.0F, track.y - 12.0F, 6.0F, track.height + 24.0F},
                       Color{0.98F, 0.90F, 0.58F, 0.95F});

    const Rect response_track{
        .x = viewport.x + (viewport.width * 0.34F),
        .y = track.y + 84.0F,
        .width = 28.0F,
        .height = viewport.height * 0.28F,
    };
    renderer.fill_rect(response_track, Color{0.15F, 0.18F, 0.22F, 1.0F});
    renderer.stroke_rect(response_track, 2.0F, Color{0.26F, 0.31F, 0.36F, 1.0F});

    const auto response_ratio = (response_ - 0.5F) / 7.5F;
    const auto response_height = (response_track.height - 10.0F) * std::clamp(response_ratio, 0.0F, 1.0F);
    renderer.fill_rect(
        Rect{response_track.x + 5.0F, response_track.y + response_track.height - response_height - 5.0F,
             response_track.width - 10.0F, response_height},
        Color{0.94F, 0.79F, 0.31F, 0.95F});

    const Rect status_row{
        .x = viewport.x + (viewport.width * 0.34F),
        .y = viewport.y + viewport.height - 54.0F,
        .width = viewport.width * 0.56F,
        .height = 18.0F,
    };

    for (int index = 0; index < 4; ++index) {
        const auto intensity = motion_enabled_
            ? (index == 0 ? 1.0F : 0.22F + (pulse * 0.18F))
            : (index == 0 ? 0.16F : 0.08F);
        const auto box_width = (status_row.width - 18.0F) / 4.0F;
        const auto box_x = status_row.x + (static_cast<float>(index) * (box_width + 6.0F));
        renderer.fill_rect(Rect{box_x, status_row.y, box_width, status_row.height},
                           Color{accent.red, accent.green, accent.blue, intensity});
    }
}

} // namespace roberta::pendant
