#pragma once

#include "raylib.h"

#include "commands.hpp"
#include "world.hpp"

// Returns the id of a dummy whose circle contains p, or -1. Used by input picking.
int dummy_at(const World& w, Vector2 p);

// Advances the whole simulation by one fixed step (ordered systems, spec section 4).
void update(World& w, const Commands& cmds, float dt);
