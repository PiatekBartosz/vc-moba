#pragma once

#include "raylib.h"

#include "world.hpp"

// Draws one frame: the tilted 3D scene, billboarded HP bars / damage numbers, the
// HUD ability bar, and the custom cursor. champ_pos/alpha carry render interpolation.
void render(const World& w, Vector2 champ_pos, float alpha, bool attack_move_armed,
            bool ignite_armed, const Camera3D& camera);
