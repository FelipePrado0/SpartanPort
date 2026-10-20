// spartanport: SDL3 window for the Vulkan swapchain (X11 or Wayland).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "sdl_window.h"
#include "spartanport_overlay.h"
#include "spartanport_graphics.h"
#include "spartanport_settings.h"

namespace Frontend {

// spartanport: set by the pause key (any thread), cleared by it or a click in the window.
static std::atomic<bool> mouse_paused{false};

void ToggleMousePause() {
    mouse_paused.store(!mouse_paused.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

// spartanport: a window opens at the render resolution chosen in the launcher (or the in-game menu); one too
// large for the screen's usable area (with room for the title bar) opens maximized, restoring to that
// size shrunk to fit. The picture is scaled to the window either way. True when shrunk.
static bool StartSize(s32& width, s32& height) {
    constexpr int title_room = 48;
    const std::vector<std::string> names = SpartanGraphics::ResolutionNames();
    const int index = std::clamp(SpartanSettings::Get().render_resolution.load(), 0, int(names.size()) - 1);
    const auto [w, h] = SpartanGraphics::ResolutionSize(names[index]);
    double scale = 1.0;
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) && usable.w > 0 && usable.h > title_room) {
        scale = std::min({1.0, double(usable.w) / w, double(usable.h - title_room) / h});
    }
    width = std::max(320, int(w * scale));
    height = std::max(180, int(h * scale));
    return scale < 1.0;
}

WindowSDL::WindowSDL(s32 width_, s32 height_, const char* title) : width{width_}, height{height_} {
    // Gamepads are sampled by runtime_pad.c; their events are pumped here with the window's.
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        UNREACHABLE_MSG("Failed to initialize SDL video: {}", SDL_GetError());
    }
    const bool too_large = StartSize(width_, height_);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width_);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height_);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, too_large);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    base_title = title;
    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    ASSERT_MSG(window, "Failed to create window: {}", SDL_GetError());
    // spartanport: the game's own icon (run.py converts sce_sys/icon0.png to BMP, SDL's built-in format).
    if (const char* icon_path = std::getenv("SPARTAN_WINDOW_ICON")) {
        if (SDL_Surface* icon = SDL_LoadBMP(icon_path)) {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        }
    }

    const char* driver = SDL_GetCurrentVideoDriver();
    const SDL_PropertiesID wp = SDL_GetWindowProperties(window);
#ifdef _WIN32
    if (driver && !std::strcmp(driver, "windows")) {
        window_info.type = WindowSystemType::Windows;
        window_info.render_surface = SDL_GetPointerProperty(wp, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    } else
#endif
    if (driver && !std::strcmp(driver, "x11")) {
        window_info.type = WindowSystemType::X11;
        window_info.display_connection = SDL_GetPointerProperty(wp, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        window_info.render_surface = reinterpret_cast<void*>(SDL_GetNumberProperty(wp, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    } else if (driver && !std::strcmp(driver, "wayland")) {
        window_info.type = WindowSystemType::Wayland;
        window_info.display_connection = SDL_GetPointerProperty(wp, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        window_info.render_surface = SDL_GetPointerProperty(wp, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    } else {
        UNREACHABLE_MSG("Unsupported SDL video driver {}", driver ? driver : "(none)");
    }
    // spartanport: SPARTAN_FULLSCREEN=1 (older launchers) still asks for fullscreen.
    if (const char* fullscreen = std::getenv("SPARTAN_FULLSCREEN"); fullscreen && fullscreen[0] == '1' &&
        SpartanSettings::Get().display_mode == SpartanGraphics::Windowed) {
        SpartanSettings::Get().display_mode = SpartanGraphics::Borderless;
    }
    ApplyDisplayMode(SpartanSettings::Get().display_mode);
    // spartanport: the game has no mouse; the in-game menu draws its own cursor (SpartanOverlay::SetOpen).
    SDL_HideCursor();
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    width = w;
    height = h;
    LOG_INFO(Frontend, "Window {}x{} on {}", w, h, driver);
}

// Windowed, borderless (desktop-sized, no mode change) or exclusive fullscreen at the desktop
// mode. Window thread only (SDL). The swapchain follows the size events.
void WindowSDL::ApplyDisplayMode(int mode) {
    display_mode = mode;
    if (mode == SpartanGraphics::Windowed) {
        SDL_SetWindowFullscreen(window, false);
    } else {
        const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
        const bool exclusive = mode == SpartanGraphics::Fullscreen && desktop;
        if (!SDL_SetWindowFullscreenMode(window, exclusive ? desktop : nullptr)) {
            LOG_WARNING(Frontend, "Fullscreen mode: {}", SDL_GetError());
        }
        SDL_SetWindowFullscreen(window, true);
    }
    SDL_SyncWindow(window);
    std::printf("Window: %s\n", SpartanGraphics::DisplayModeNames[mode]);
}

WindowSDL::~WindowSDL() {
    SDL_DestroyWindow(window);
}

void WindowSDL::BeginTextInput(const std::string& initial, const std::string& prompt) {
    std::scoped_lock lock{text_mutex};
    text = initial;
    text_prompt = prompt;
    text_state = 0;
    text_requested = true;
}

int WindowSDL::PollTextInput(std::string& out) {
    std::scoped_lock lock{text_mutex};
    out = text;
    return text_state;
}

void WindowSDL::UpdateTextTitle() {
    const std::string title = text_active ? base_title + " \u2014 " + text_prompt + ": " + text + "_  (Enter = OK, Esc = cancel)"
                                          : base_title;
    SDL_SetWindowTitle(window, title.c_str());
    SpartanOverlay::SetTextEntry(text_active, text_prompt, text);
}

bool WindowSDL::PollEvents() {
    {
        std::scoped_lock lock{text_mutex};
        if (text_requested) { // SDL text input must be toggled from the window thread
            text_requested = false;
            text_active = true;
            SDL_StartTextInput(window);
            UpdateTextTitle();
        }
    }
    if (!text_active) {
        SpartanOverlay::UpdateTextInput(window);
    }
    if (const int mode = SpartanSettings::Get().display_mode; mode != display_mode) {
        ApplyDisplayMode(mode);
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (text_active && (event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_KEY_DOWN)) {
            std::scoped_lock lock{text_mutex};
            if (event.type == SDL_EVENT_TEXT_INPUT) {
                text += event.text.text;
            } else if (event.key.key == SDLK_BACKSPACE && !text.empty()) {
                size_t cut = text.size() - 1; // drop one UTF-8 code point
                while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
                text.erase(cut);
            } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_ESCAPE) {
                text_state = event.key.key == SDLK_ESCAPE ? 2 : 1;
                text_active = false;
                SDL_StopTextInput(window);
            }
            UpdateTextTitle();
            continue;
        }
        // The controller finishes the text dialog too: Cross (A) accepts, Circle (B) cancels.
        if (text_active && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH ||
             event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)) {
            std::scoped_lock lock{text_mutex};
            text_state = event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH ? 1 : 2;
            text_active = false;
            SDL_StopTextInput(window);
            UpdateTextTitle();
            continue;
        }
        if (SpartanOverlay::HandleEvent(event)) {
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED: {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            width = w;
            height = h;
            break;
        }
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            is_open = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouse_paused = false; // a click in the window takes the mouse back
            break;
        default:
            break;
        }
    }
    // spartanport: while the game has the mouse it stays in the window (relative mode: hidden, no edges,
    // so a click cannot land on another monitor); the menu, the save list, a text dialog,
    // another window or the game's pause menu (the pause key) get it back. Taken back, it starts
    // at the window's centre; let go by the pause key, the system cursor shows.
    const bool paused = mouse_paused.load(std::memory_order_relaxed);
    const bool lock = SpartanSettings::Get().mouse_lock.load(std::memory_order_relaxed) && !text_active && !paused &&
                      !SpartanOverlay::CapturesInput() && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
    if (lock != mouse_locked) {
        if (lock) SDL_WarpMouseInWindow(window, width / 2.0f, height / 2.0f);
        if (SDL_SetWindowRelativeMouseMode(window, lock)) mouse_locked = lock;
    }
    if (const bool show = paused && !mouse_locked; show != cursor_shown) {
        show ? SDL_ShowCursor() : SDL_HideCursor();
        cursor_shown = show;
    }
    return is_open;
}

} // namespace Frontend
