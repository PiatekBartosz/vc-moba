#include "raylib.h"

inline constexpr int k_window_size_x = 1920;
inline constexpr int k_window_size_y = 1080;
inline constexpr int k_target_fps = 160;

int main() {
    InitWindow(k_window_size_x, k_window_size_y, "Practice Tool");
    SetTargetFPS(k_target_fps);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{18, 18, 22, 255});
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
