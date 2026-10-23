// SPDX-License-Identifier: GPL-2.0-or-later
// Tests for configurable Cross/Circle confirmation behavior (Issue #934).
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sstream>
#include <SDL3/SDL.h>
#include "spartanport_settings.h"
#include "../gpu/spartanportgpu.h"
#include "imgui.h"

// Forward declaration of KeyFromGamepad from spartanport_overlay.cpp / test harness
namespace SpartanTest {

ImGuiKey KeyFromGamepad(uint8_t button) {
    const bool swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return swapped ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
    case SDL_GAMEPAD_BUTTON_EAST: return swapped ? ImGuiKey_GamepadFaceDown : ImGuiKey_GamepadFaceRight;
    case SDL_GAMEPAD_BUTTON_WEST: return ImGuiKey_GamepadFaceLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return ImGuiKey_GamepadFaceUp;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return ImGuiKey_GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ImGuiKey_GamepadL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
    case SDL_GAMEPAD_BUTTON_START: return ImGuiKey_GamepadStart;
    case SDL_GAMEPAD_BUTTON_BACK: return ImGuiKey_GamepadBack;
    default: return ImGuiKey_None;
    }
}

// Simulates HandleChoiceEvent decision logic from gpu/shim/spartanport_overlay.cpp
int EvaluateChoiceEvent(const SDL_Event& event) {
    int finish = 0; // 1 accept, 2 cancel
    if (event.type == SDL_EVENT_KEY_DOWN) {
        const SDL_Keycode key = event.key.key;
        finish = key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE ? 1
                 : key == SDLK_ESCAPE || key == SDLK_BACKSPACE                   ? 2
                                                                                 : 0;
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        const uint8_t button = event.gbutton.button;
        const bool swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
        const uint8_t confirm_btn = swapped ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
        const uint8_t cancel_btn = swapped ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
        finish = button == confirm_btn ? 1 : button == cancel_btn ? 2 : 0;
    }
    return finish;
}

// Simulates window.cpp text input dialog decision logic
int EvaluateTextDialogEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
        (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH ||
         event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)) {
        const bool swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
        const uint8_t confirm_btn = swapped ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
        return event.gbutton.button == confirm_btn ? 1 : 2;
    }
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) return 1;
        if (event.key.key == SDLK_ESCAPE) return 2;
    }
    return 0;
}

} // namespace SpartanTest

static int SetEnv(const char* name, const char* value) {
#if defined(_WIN32)
    return _putenv_s(name, value);
#else
    return value[0] ? setenv(name, value, 1) : unsetenv(name);
#endif
}

static std::string ReadFile(const char* path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main() {
    auto& s = SpartanSettings::Get();
    const char* config_path = "menu-confirm-test.ini";
    SetEnv("SPARTAN_CONFIG", config_path);

    // 1. Verify default values
    s.menu_confirm_swap = 0;
    assert(s.menu_confirm_swap == 0);
    assert(spartanportgpu_menu_confirm_swap() == 0);

    // 2. Choice dialog events: Default Cross confirms, Circle cancels
    {
        SDL_Event ev_south{};
        ev_south.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        ev_south.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        assert(SpartanTest::EvaluateChoiceEvent(ev_south) == 1); // Accept

        SDL_Event ev_east{};
        ev_east.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        ev_east.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
        assert(SpartanTest::EvaluateChoiceEvent(ev_east) == 2); // Cancel

        // Keyboard Enter and Space accept, Escape and Backspace cancel
        SDL_Event ev_ret{};
        ev_ret.type = SDL_EVENT_KEY_DOWN;
        ev_ret.key.key = SDLK_RETURN;
        assert(SpartanTest::EvaluateChoiceEvent(ev_ret) == 1);

        SDL_Event ev_space{};
        ev_space.type = SDL_EVENT_KEY_DOWN;
        ev_space.key.key = SDLK_SPACE;
        assert(SpartanTest::EvaluateChoiceEvent(ev_space) == 1);

        SDL_Event ev_esc{};
        ev_esc.type = SDL_EVENT_KEY_DOWN;
        ev_esc.key.key = SDLK_ESCAPE;
        assert(SpartanTest::EvaluateChoiceEvent(ev_esc) == 2);

        SDL_Event ev_bksp{};
        ev_bksp.type = SDL_EVENT_KEY_DOWN;
        ev_bksp.key.key = SDLK_BACKSPACE;
        assert(SpartanTest::EvaluateChoiceEvent(ev_bksp) == 2);

        // Text entry dialog: South accepts, East cancels
        assert(SpartanTest::EvaluateTextDialogEvent(ev_south) == 1);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_east) == 2);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_ret) == 1);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_esc) == 2);

        // ImGui navigation: South is FaceDown (confirm), East is FaceRight (cancel)
        assert(SpartanTest::KeyFromGamepad(SDL_GAMEPAD_BUTTON_SOUTH) == ImGuiKey_GamepadFaceDown);
        assert(SpartanTest::KeyFromGamepad(SDL_GAMEPAD_BUTTON_EAST) == ImGuiKey_GamepadFaceRight);
    }

    // 3. Choice dialog events: Swapped mode (menu_confirm_swap = 1)
    s.menu_confirm_swap = 1;
    assert(spartanportgpu_menu_confirm_swap() == 1);
    {
        SDL_Event ev_south{};
        ev_south.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        ev_south.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        assert(SpartanTest::EvaluateChoiceEvent(ev_south) == 2); // Now cancel!

        SDL_Event ev_east{};
        ev_east.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        ev_east.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
        assert(SpartanTest::EvaluateChoiceEvent(ev_east) == 1); // Now accept!

        // Keyboard inputs remain completely unchanged
        SDL_Event ev_ret{};
        ev_ret.type = SDL_EVENT_KEY_DOWN;
        ev_ret.key.key = SDLK_RETURN;
        assert(SpartanTest::EvaluateChoiceEvent(ev_ret) == 1);

        SDL_Event ev_space{};
        ev_space.type = SDL_EVENT_KEY_DOWN;
        ev_space.key.key = SDLK_SPACE;
        assert(SpartanTest::EvaluateChoiceEvent(ev_space) == 1);

        SDL_Event ev_esc{};
        ev_esc.type = SDL_EVENT_KEY_DOWN;
        ev_esc.key.key = SDLK_ESCAPE;
        assert(SpartanTest::EvaluateChoiceEvent(ev_esc) == 2);

        // Text entry dialog: East accepts, South cancels
        assert(SpartanTest::EvaluateTextDialogEvent(ev_south) == 2);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_east) == 1);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_ret) == 1);
        assert(SpartanTest::EvaluateTextDialogEvent(ev_esc) == 2);

        // ImGui navigation: South is FaceRight (cancel), East is FaceDown (confirm)
        assert(SpartanTest::KeyFromGamepad(SDL_GAMEPAD_BUTTON_SOUTH) == ImGuiKey_GamepadFaceRight);
        assert(SpartanTest::KeyFromGamepad(SDL_GAMEPAD_BUTTON_EAST) == ImGuiKey_GamepadFaceDown);
    }

    // 4. Persistence in spartanport.ini
    {
        std::ofstream out(config_path);
        out << "menu_confirm_swap=1\n";
    }
    s.menu_confirm_swap = 0;
    SpartanSettings::Load();
    assert(s.menu_confirm_swap == 1);
    assert(spartanportgpu_menu_confirm_swap() == 1);

    SpartanSettings::Save();
    assert(ReadFile(config_path).find("menu_confirm_swap=1\n") != std::string::npos);

    // Reading alternate key "menu_confirm=circle"
    {
        std::ofstream out(config_path);
        out << "menu_confirm=circle\n";
    }
    s.menu_confirm_swap = 0;
    SpartanSettings::Load();
    assert(s.menu_confirm_swap == 1);

    // Reading "menu_confirm_swap=0"
    {
        std::ofstream out(config_path);
        out << "menu_confirm_swap=0\n";
    }
    s.menu_confirm_swap = 1;
    SpartanSettings::Load();
    assert(s.menu_confirm_swap == 0);
    assert(spartanportgpu_menu_confirm_swap() == 0);

    // 5. Environment variable override
    SetEnv("SPARTAN_MENU_CONFIRM_SWAP", "1");
    SpartanSettings::Load();
    assert(s.menu_confirm_swap == 1);
    SetEnv("SPARTAN_MENU_CONFIRM_SWAP", "");

    // Clean up temp file
    std::remove(config_path);

    std::puts("PASS: menu confirm swap tests");
    return 0;
}
