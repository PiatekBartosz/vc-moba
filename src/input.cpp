#include "input.hpp"

#include "raymath.h"

#include "systems.hpp"  // dummy_at

// Project the mouse cursor onto the ground plane (y = 0), returning world 2D coords.
Vector2 mouse_ground(const Camera3D& camera) {
    const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    const float t = fabsf(ray.direction.y) > 1e-6f ? -ray.position.y / ray.direction.y : 0.0f;
    const Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return Vector2{hit.x, hit.z};
}

void poll_input(const World& w, Commands& cmds, InputState& in, const Camera3D& camera) {
    // Hold-to-move (LoL style): while the right button is held, keep re-issuing the
    // order toward the cursor. Right-click on a dummy attacks it; on ground, moves.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 wp = mouse_ground(camera);
        const int hit = dummy_at(w, wp);
        if (hit >= 0) {
            cmds.attack_requested = true;
            cmds.attack_target_id = hit;
        } else {
            cmds.move_requested = true;
            cmds.move_point = wp;
        }
    }

    // Attack-move: press A to arm, then left-click a point. Right-click / Esc cancels.
    if (IsKeyPressed(KEY_A)) {
        in.attack_move_armed = true;
    }
    if (in.attack_move_armed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        cmds.attack_move_requested = true;
        cmds.attack_move_point = mouse_ground(camera);
        in.attack_move_armed = false;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        in.attack_move_armed = false;  // Esc is reserved for the pause menu
    }

    // Vayne abilities.
    if (IsKeyPressed(KEY_Q)) {
        cmds.q_requested = true;
        cmds.q_point = mouse_ground(camera);
    }
    if (IsKeyPressed(KEY_E)) {
        cmds.e_requested = true;
    }
    if (IsKeyPressed(KEY_R)) {
        cmds.ult_requested = true;
    }

    if (IsKeyPressed(KEY_T)) {
        cmds.place_requested = true;
        cmds.place_point = mouse_ground(camera);
    }
    if (IsKeyPressed(KEY_D)) {
        cmds.flash_requested = true;
        cmds.flash_point = mouse_ground(camera);
    }
    if (IsKeyPressed(KEY_F)) {
        cmds.ghost_requested = true;
    }
    if (IsKeyPressed(KEY_N)) {
        cmds.toggle_cooldowns = true;
    }
    if (IsKeyPressed(KEY_X)) {
        cmds.reset_requested = true;
    }
}
