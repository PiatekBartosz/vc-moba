#pragma once

#include "raylib.h"

#include "commands.hpp"
#include "world.hpp"

// Translates raw raylib input into Commands for the current frame, updating the
// persistent InputState (armed targeting modes). Reads the world for mouse picking.
void poll_input(const World& w, Commands& cmds, InputState& in, const Camera3D& camera);
