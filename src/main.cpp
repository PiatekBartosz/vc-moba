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
    world.walls.push_back(Rectangle{300.0f, -180.0f, 70.0f, 360.0f});  // flash over this
    world.walls.push_back(Rectangle{-560.0f, 220.0f, 380.0f, 70.0f});
    world.walls.push_back(Rectangle{-600.0f, -380.0f, 70.0f, 320.0f});

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
