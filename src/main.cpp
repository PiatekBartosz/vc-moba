#include "raylib.h"
#include "raymath.h"

#include "commands.hpp"
#include "config.hpp"
#include "input.hpp"
#include "render.hpp"
#include "systems.hpp"
#include "world.hpp"

int main() {
    InitWindow(k_window_size_x, k_window_size_y, "Practice Tool");
    SetTargetFPS(k_target_fps);
    HideCursor();  // we draw our own cursor in the renderer

    World world{};
    // Jungle rock walls, laid out with 180-degree rotational symmetry like the Rift.
    // Each call adds a wall and its mirror through the map center.
    const auto add_wall_pair = [&world](float x, float z, float w, float d) {
        world.walls.push_back(Rectangle{x, z, w, d});
        world.walls.push_back(Rectangle{-(x + w), -(z + d), w, d});
    };
    add_wall_pair(-1560.0f, 640.0f, 110.0f, 460.0f);   // outer jungle wall by blue base
    add_wall_pair(-1320.0f, 1180.0f, 430.0f, 110.0f);  // base mouth
    add_wall_pair(-980.0f, 690.0f, 110.0f, 330.0f);    // raptors/wolf divider
    add_wall_pair(-560.0f, 1120.0f, 360.0f, 110.0f);   // wall toward bot lane
    add_wall_pair(-1180.0f, 200.0f, 110.0f, 320.0f);   // wall near river
    add_wall_pair(-470.0f, 470.0f, 300.0f, 110.0f);    // mid-lane jungle flank

    Camera3D camera{};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = k_cam_fovy;
    camera.projection = CAMERA_PERSPECTIVE;
    const Vector3 champ_start = to3d(world.champ.pos, 0.0f);
    camera.target = champ_start;
    camera.position = Vector3Add(champ_start, k_cam_offset);

    Commands cmds{};
    InputState input{};
    float acc = 0.0f;
    while (!WindowShouldClose()) {
        // Clamp to avoid a spiral of death if a frame hitches (e.g. window drag).
        acc += fminf(GetFrameTime(), 0.25f);
        poll_input(world, cmds, input, camera);

        bool stepped = false;
        while (acc >= k_dt) {
            update(world, cmds, k_dt);
            cmds.consume_one_shots();  // one-shots fire once, on the first substep
            acc -= k_dt;
            stepped = true;
        }
        if (stepped) {
            cmds.clear_orders();  // move/attack orders are re-latched each frame while held
        }

        // Render interpolation: draw between the last two sim states by how far we
        // are into the next fixed step, so motion is smooth at any render rate.
        const float alpha = acc / k_dt;
        const Vector2 champ_pos = Vector2Lerp(world.champ.prev_pos, world.champ.pos, alpha);

        const Vector3 champ3 = to3d(champ_pos, 0.0f);
        camera.target = champ3;
        camera.position = Vector3Add(champ3, k_cam_offset);
        render(world, champ_pos, alpha, input.attack_move_armed, input.ignite_armed, camera);
    }

    CloseWindow();
    return 0;
}
