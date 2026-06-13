#pragma once

#include "raylib.h"

#include "app.hpp"
#include "world.hpp"

// Draws one frame: the tilted 3D scene, billboarded HP bars / damage numbers, the
// HUD ability bar, the custom cursor, and the editor/menu overlays.
// champ_pos/alpha carry render interpolation; cursor_ground is the mouse->ground point.
void render(const World& w, Vector2 champ_pos, float alpha, bool attack_move_armed,
            bool ignite_armed, const Camera3D& camera, const AppState& app,
            Vector2 cursor_ground);
