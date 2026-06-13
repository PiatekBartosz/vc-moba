#pragma once

#include "raylib.h"

#include "commands.hpp"
#include "world.hpp"

// Projects the mouse cursor onto the ground plane (y = 0) -> world 2D coords.
Vector2 mouse_ground(const Camera3D& camera);

// Translates raw raylib input into Commands for the current frame, updating the
// persistent InputState (armed targeting modes). Reads the world for mouse picking.
void poll_input(const World& w, Commands& cmds, InputState& in, const Camera3D& camera);
