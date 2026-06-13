#include "input.hpp"

#include "raymath.h"

#include "systems.hpp"  // dummy_at

// Project the mouse cursor onto the ground plane (y = 0), returning world 2D coords.
static Vector2 mouse_ground(const Camera3D& camera) {
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

    // Targeting modes: press A (attack-move) or R (ignite) to arm, then left-click.
    // Arming is mutually exclusive; right-click / Esc cancels.
    if (IsKeyPressed(KEY_A)) {
        in.attack_move_armed = true;
        in.ignite_armed = false;
    }
    if (IsKeyPressed(KEY_R)) {
        in.ignite_armed = true;
        in.attack_move_armed = false;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 wp = mouse_ground(camera);
        if (in.attack_move_armed) {
            cmds.attack_move_requested = true;
            cmds.attack_move_point = wp;
        } else if (in.ignite_armed) {
            const int hit = dummy_at(w, wp);
            if (hit >= 0) {
                cmds.ignite_requested = true;
                cmds.ignite_target_id = hit;
            }
        }
        in.attack_move_armed = false;
        in.ignite_armed = false;  // a left-click resolves (or wastes) the armed mode
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
        in.attack_move_armed = false;
        in.ignite_armed = false;
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
