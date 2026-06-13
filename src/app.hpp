#pragma once

#include "raylib.h"

#include "config.hpp"

// Top-level application state: the pause menu and the map-creator editor.

enum class Mode { Normal, Practice, Editor };  // Normal == survival game
enum class Tool { Wall, Dummy, Erase };

struct AppState {
    bool menu_open = false;
    Mode mode = Mode::Practice;
    Tool tool = Tool::Wall;
    bool dragging = false;          // mid wall drag in the editor
    Vector2 drag_start{0.0f, 0.0f};
    Vector2 edit_cam{0.0f, 0.0f};   // editor free-camera target (world coords)

    Vector2 cam{0.0f, 0.0f};        // normal-mode camera target (world coords)
    bool cam_locked = true;         // true == follow the champion; false == free (edge-panned)
};

inline constexpr int k_menu_buttons = 5;

inline Rectangle menu_button_rect(const int i) {
    const float bw = 340.0f;
    const float bh = 64.0f;
    const float gap = 18.0f;
    const float total = static_cast<float>(k_menu_buttons) * bh +
                        static_cast<float>(k_menu_buttons - 1) * gap;
    const float x = (static_cast<float>(GetScreenWidth()) - bw) * 0.5f;
    const float y0 = (static_cast<float>(GetScreenHeight()) - total) * 0.5f + 30.0f;
    return Rectangle{x, y0 + static_cast<float>(i) * (bh + gap), bw, bh};
}

inline const char* menu_button_label(const int i) {
    switch (i) {
        case 0:
            return "Resume";
        case 1:
            return "Normal (Survival)";
        case 2:
            return "Practice Tool";
        case 3:
            return "Map Creator";
        default:
            return "Quit";
    }
}
