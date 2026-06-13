#include "raylib.h"

inline constexpr int k_window_size_x = 1920;
inline constexpr int k_window_size_y = 1080;
inline constexpr int k_target_fps = 160;

inline constexpr float k_dt = 1.0f / 60.0f;

static void update(const float dt) {
    (void)dt;
}

static void render() {
    BeginDrawing();
    ClearBackground(Color{18, 18, 22, 255});
    DrawFPS(10, 10);
    EndDrawing();
}

int main() {
    InitWindow(k_window_size_x, k_window_size_y, "Practice Tool");
    SetTargetFPS(k_target_fps);

    float acc = 0.0f;
    while (!WindowShouldClose()) {
        acc += GetFrameTime();
        while (acc >= k_dt) {
            update(k_dt);
            acc -= k_dt;
        }
        render();
    }

    CloseWindow();
    return 0;
}
