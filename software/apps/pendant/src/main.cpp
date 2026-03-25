#include "mesh_renderer.hpp"
#include "robot_view.hpp"
#include "text_renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

[[nodiscard]] constexpr const char* pendant_app_id()
{
#if defined(ROBERTA_PENDANT_TARGET_EMBEDDED_LINUX)
    return "roberta_pendant_embedded_linux";
#elif defined(ROBERTA_PENDANT_TARGET_MACOS_DEV)
    return "roberta_pendant_macos_dev";
#else
    return "roberta_pendant";
#endif
}

[[nodiscard]] constexpr const char* pendant_renderer_label()
{
#if defined(ROBERTA_PENDANT_USE_GLES2)
    return "SDL2 + OpenGL ES 2.0";
#else
    return "SDL2 + desktop OpenGL";
#endif
}

[[nodiscard]] constexpr const char* pendant_window_title()
{
#if defined(ROBERTA_PENDANT_TARGET_MACOS_DEV)
    return "Roberta Pendant Dev";
#else
    return "Roberta Pendant";
#endif
}

void draw_overlay_text(roberta::pendant::TextRenderer& text_renderer)
{
    using roberta::pendant::Color;
    using roberta::pendant::FontFace;
    using roberta::pendant::TextStyle;

    const TextStyle title_style{
        .font = FontFace::sans,
        .pixel_height = 28.0F,
        .line_spacing = 1.1F,
        .color = Color{0.95F, 0.97F, 1.0F, 0.94F},
    };
    const TextStyle detail_style{
        .font = FontFace::mono,
        .pixel_height = 16.0F,
        .line_spacing = 1.25F,
        .color = Color{0.84F, 0.89F, 0.96F, 0.88F},
    };
    const TextStyle shadow_style{
        .font = FontFace::sans,
        .pixel_height = title_style.pixel_height,
        .line_spacing = title_style.line_spacing,
        .color = Color{0.02F, 0.03F, 0.04F, 0.72F},
    };
    const TextStyle detail_shadow_style{
        .font = FontFace::mono,
        .pixel_height = detail_style.pixel_height,
        .line_spacing = detail_style.line_spacing,
        .color = Color{0.02F, 0.03F, 0.04F, 0.58F},
    };

    text_renderer.draw_text("Roberta Pendant", 24.0F + 2.0F, 20.0F + 2.0F, shadow_style);
    text_renderer.draw_text("Roberta Pendant", 24.0F, 20.0F, title_style);

    const auto renderer_line = std::string("Renderer: ") + pendant_renderer_label();
    text_renderer.draw_text(renderer_line, 24.0F + 1.0F, 56.0F + 1.0F, detail_shadow_style);
    text_renderer.draw_text(renderer_line, 24.0F, 56.0F, detail_style);

    text_renderer.draw_text("Fonts: Geist Regular | Geist Mono Regular", 24.0F + 1.0F, 78.0F + 1.0F,
                            detail_shadow_style);
    text_renderer.draw_text("Fonts: Geist Regular | Geist Mono Regular", 24.0F, 78.0F, detail_style);

    text_renderer.draw_text("Keys: ARROWS/KP4 KP6 KP8 KP2  PERIOD  MINUS  SPACE  R  ESC", 24.0F + 1.0F,
                            100.0F + 1.0F, detail_shadow_style);
    text_renderer.draw_text("Keys: ARROWS/KP4 KP6 KP8 KP2  PERIOD  MINUS  SPACE  R  ESC", 24.0F, 100.0F, detail_style);
}

class SdlSession final {
public:
    SdlSession()
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
    }

    ~SdlSession()
    {
        SDL_Quit();
    }

    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;
};

struct SdlWindowDeleter final {
    void operator()(SDL_Window* window) const
    {
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
    }
};

class GlContext final {
public:
    explicit GlContext(SDL_Window* window)
        : context_(SDL_GL_CreateContext(window))
    {
        if (context_ == nullptr) {
            throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
        }
    }

    ~GlContext()
    {
        if (context_ != nullptr) {
            SDL_GL_DeleteContext(context_);
        }
    }

    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;

    [[nodiscard]] SDL_GLContext native_handle() const noexcept
    {
        return context_;
    }

private:
    SDL_GLContext context_{nullptr};
};

void print_banner()
{
    std::cout
        << pendant_app_id() << '\n'
        << "renderer: " << pendant_renderer_label() << '\n'
        << "controls:\n"
        << "  Left / Keypad 4   orbit left\n"
        << "  Right / Keypad 6  orbit right\n"
        << "  Up / Keypad 8     orbit up\n"
        << "  Down / Keypad 2   orbit down\n"
        << "  Period            zoom in\n"
        << "  Minus             zoom out\n"
        << "  Space             pause/resume robot motion\n"
        << "  R                 reset camera\n"
        << "  Escape / Q        quit\n";
}

void set_gl_attributes()
{
#if defined(ROBERTA_PENDANT_USE_GLES2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
}

} // namespace

int main()
{
    print_banner();

    try {
        SdlSession sdl_session;
        set_gl_attributes();

        using WindowHandle = std::unique_ptr<SDL_Window, SdlWindowDeleter>;

        WindowHandle window(SDL_CreateWindow(pendant_window_title(), SDL_WINDOWPOS_CENTERED,
                                             SDL_WINDOWPOS_CENTERED, 1280, 720,
                                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
        if (!window) {
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        }

        GlContext gl_context(window.get());

        if (SDL_GL_MakeCurrent(window.get(), gl_context.native_handle()) != 0) {
            throw std::runtime_error(std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
        }

        SDL_GL_SetSwapInterval(1);

        roberta::pendant::MeshRenderer renderer;
        roberta::pendant::RobotView robot_view(renderer);
        roberta::pendant::TextRenderer text_renderer;

        auto previous_tick = std::chrono::steady_clock::now();
        bool running = true;

        while (running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event) == 1) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    continue;
                }

                if (event.type != SDL_KEYDOWN || event.key.repeat != 0) {
                    continue;
                }

                switch (event.key.keysym.sym) {
                case SDLK_q:
                case SDLK_ESCAPE:
                    running = false;
                    break;
                default:
                    robot_view.handle_keydown(event.key.keysym.sym);
                    break;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            const auto delta_seconds =
                std::chrono::duration<float>(now - previous_tick).count();
            previous_tick = now;

            robot_view.update(std::min(delta_seconds, 0.1F));

            int drawable_width = 0;
            int drawable_height = 0;
            SDL_GL_GetDrawableSize(window.get(), &drawable_width, &drawable_height);

            renderer.set_viewport(drawable_width, drawable_height);
            renderer.begin_frame(roberta::pendant::Vec4{0.04F, 0.05F, 0.07F, 1.0F});
            robot_view.draw(renderer, drawable_width, drawable_height);
            text_renderer.set_viewport(drawable_width, drawable_height);
            draw_overlay_text(text_renderer);

            SDL_GL_SwapWindow(window.get());
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
