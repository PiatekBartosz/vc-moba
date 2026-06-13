#include "raylib.h"
#include "raymath.h"

#include <cstddef>
#include <vector>

#include "app.hpp"
#include "commands.hpp"
#include "config.hpp"
#include "input.hpp"
#include "render.hpp"
#include "systems.hpp"
#include "world.hpp"

// Pause-menu clicks: act on whichever button was pressed.
static void handle_menu(AppState& app, World& world, bool& quit) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }
    const Vector2 m = GetMousePosition();
    for (int i = 0; i < k_menu_buttons; ++i) {
        if (!CheckCollisionPointRec(m, menu_button_rect(i))) {
            continue;
        }
        switch (i) {
            case 0:  // Resume
                app.menu_open = false;
                break;
            case 1:  // Normal (Survival)
                app.mode = Mode::Normal;
                start_survival(world);
                app.menu_open = false;
                break;
            case 2:  // Practice Tool
                app.mode = Mode::Practice;
                start_practice(world);
                app.menu_open = false;
                break;
            case 3:  // Map Creator
                app.mode = Mode::Editor;
                app.menu_open = false;
                break;
            default:  // Quit
                quit = true;
                break;
        }
    }
}

// Map-creator input: pan the camera, pick a tool, and place/erase on the ground.
static void handle_editor(AppState& app, World& world, const Camera3D& camera) {
    const float pan = 1100.0f * GetFrameTime();
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        app.edit_cam.y -= pan;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        app.edit_cam.y += pan;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        app.edit_cam.x -= pan;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        app.edit_cam.x += pan;
    }

    if (IsKeyPressed(KEY_ONE)) {
        app.tool = Tool::Wall;
    }
    if (IsKeyPressed(KEY_TWO)) {
        app.tool = Tool::Dummy;
    }
    if (IsKeyPressed(KEY_THREE)) {
        app.tool = Tool::Erase;
    }

    const Vector2 g = mouse_ground(camera);

    if (app.tool == Tool::Wall) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            app.dragging = true;
            app.drag_start = g;
        }
        if (app.dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const float x = fminf(app.drag_start.x, g.x);
            const float z = fminf(app.drag_start.y, g.y);
            const float ww = fabsf(g.x - app.drag_start.x);
            const float hh = fabsf(g.y - app.drag_start.y);
            if (ww > 20.0f && hh > 20.0f) {
                world.walls.push_back(Rectangle{x, z, ww, hh});
            }
            app.dragging = false;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            app.dragging = false;  // cancel a drag
        }
    } else if (app.tool == Tool::Dummy) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            world.dummies.push_back(Dummy{world.next_id++, g});
        }
    } else {  // Erase
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            bool removed = false;
            for (std::size_t i = 0; i < world.walls.size(); ++i) {
                if (CheckCollisionPointRec(g, world.walls[i])) {
                    world.walls.erase(world.walls.begin() + static_cast<std::ptrdiff_t>(i));
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                const int id = dummy_at(world, g);
                if (id >= 0) {
                    std::erase_if(world.dummies, [id](const Dummy& d) { return d.id == id; });
                }
            }
        }
    }
}

// Normal-mode camera: edge-of-screen panning (free), Space recenters/locks to champ.
static void handle_camera(AppState& app, const Vector2 champ_pos) {
    if (IsKeyPressed(KEY_SPACE)) {
        app.cam_locked = true;
    }
    const float margin = 14.0f;
    const float speed = 1400.0f * GetFrameTime();
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());
    const Vector2 m = GetMousePosition();
    Vector2 dir{0.0f, 0.0f};
    if (m.x <= margin) {
        dir.x = -1.0f;
    } else if (m.x >= sw - margin) {
        dir.x = 1.0f;
    }
    if (m.y <= margin) {
        dir.y = -1.0f;
    } else if (m.y >= sh - margin) {
        dir.y = 1.0f;
    }
    if (dir.x != 0.0f || dir.y != 0.0f) {
        app.cam_locked = false;  // edge pan unlocks the camera
        app.cam.x += dir.x * speed;
        app.cam.y += dir.y * speed;
    }
    if (app.cam_locked) {
        app.cam = champ_pos;
    }
}

int main() {
    // Borderless fullscreen so the whole client area is on-screen and every screen
    // edge is reachable for edge-pan (a titled 1080p window would push the top/bottom
    // edges off-screen, breaking vertical scrolling).
    SetConfigFlags(FLAG_BORDERLESS_WINDOWED_MODE);
    InitWindow(k_window_size_x, k_window_size_y, "Practice Tool");
    SetTargetFPS(k_target_fps);
    SetExitKey(KEY_NULL);  // Esc drives our pause menu, not window close
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
    AppState app{};
    float acc = 0.0f;
    bool quit = false;
    while (!WindowShouldClose() && !quit) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            app.menu_open = !app.menu_open;
        }

        if (app.menu_open) {
            handle_menu(app, world, quit);
        } else if (app.mode == Mode::Editor) {
            handle_editor(app, world, camera);
        } else {
            world.survival = (app.mode == Mode::Normal);
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
                cmds.clear_orders();  // move/attack orders re-latched each frame while held
            }
        }

        // Pick the camera target and the champion draw position for this frame.
        Vector2 champ_pos{0.0f, 0.0f};
        float alpha = 0.0f;
        if (app.mode == Mode::Editor) {
            champ_pos = world.champ.pos;
            const Vector3 c3 = to3d(app.edit_cam, 0.0f);
            camera.target = c3;
            camera.position = Vector3Add(c3, k_cam_offset);
        } else {
            // Render interpolation between the last two sim states.
            alpha = acc / k_dt;
            champ_pos = Vector2Lerp(world.champ.prev_pos, world.champ.pos, alpha);
            if (!app.menu_open) {
                handle_camera(app, champ_pos);
            }
            const Vector3 c3 = to3d(app.cam, 0.0f);
            camera.target = c3;
            camera.position = Vector3Add(c3, k_cam_offset);
        }

        const Vector2 cursor_ground = mouse_ground(camera);
        render(world, champ_pos, alpha, input.attack_move_armed, input.ignite_armed, camera, app,
               cursor_ground);
    }

    CloseWindow();
    return 0;
}
