#pragma once

#include "raylib.h"

// Per-frame input intents handed from input -> systems, plus persistent UI state.

struct Commands {
    bool move_requested = false;
    Vector2 move_point{0.0f, 0.0f};

    bool attack_requested = false;
    int attack_target_id = -1;

    bool attack_move_requested = false;
    Vector2 attack_move_point{0.0f, 0.0f};

    bool place_requested = false;
    Vector2 place_point{0.0f, 0.0f};

    bool flash_requested = false;
    Vector2 flash_point{0.0f, 0.0f};

    bool ghost_requested = false;

    bool ignite_requested = false;
    int ignite_target_id = -1;

    bool toggle_cooldowns = false;
    bool reset_requested = false;

    // Order latches (move/attack) are re-issued each frame and cleared after the
    // substep loop. One-shots are consumed after each substep so they fire once.
    void clear_orders() {
        move_requested = false;
        attack_requested = false;
    }
    void consume_one_shots() {
        place_requested = false;
        flash_requested = false;
        ghost_requested = false;
        ignite_requested = false;
        toggle_cooldowns = false;
        reset_requested = false;
        attack_move_requested = false;
    }
};

// Cross-frame input state (UI modes that persist between frames).
struct InputState {
    bool attack_move_armed = false;  // 'A' pressed, waiting for a left-click target point
    bool ignite_armed = false;       // 'R' pressed, waiting for a left-click on a dummy
};
