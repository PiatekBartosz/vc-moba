#pragma once

#include "raylib.h"

// All gameplay/presentation tunables live here.

inline constexpr int k_window_size_x = 1920;
inline constexpr int k_window_size_y = 1080;
inline constexpr int k_target_fps = 160;

inline constexpr float k_dt = 1.0f / 60.0f;

inline constexpr float k_grid_spacing = 100.0f;
inline constexpr float k_arena_half_extent = 2000.0f;

inline constexpr float k_move_speed = 340.0f;
inline constexpr float k_champ_radius = 24.0f;
inline constexpr float k_arrive_epsilon = 1.0f;

inline constexpr float k_attack_range = 550.0f;
inline constexpr float k_attacks_per_sec = 1.6f;
inline constexpr float k_attack_damage = 60.0f;
inline constexpr float k_windup_fraction = 0.3f;  // share of the attack cycle spent winding up

inline constexpr float k_flash_range = 400.0f;
inline constexpr float k_flash_cd = 300.0f;
inline constexpr float k_ghost_bonus = 0.30f;  // +30% move speed
inline constexpr float k_ghost_duration = 10.0f;
inline constexpr float k_ghost_cd = 210.0f;
inline constexpr float k_ignite_dps = 50.0f;
inline constexpr float k_ignite_duration = 5.0f;
inline constexpr float k_ignite_cd = 180.0f;

inline constexpr float k_projectile_speed = 1800.0f;
inline constexpr float k_projectile_radius = 7.0f;
inline constexpr float k_hit_distance = 6.0f;

inline constexpr float k_popup_life = 0.8f;
inline constexpr float k_popup_rise = 70.0f;  // upward drift, units/sec

inline constexpr float k_dummy_radius = 28.0f;
inline constexpr float k_dummy_hp = 1000.0f;

// 3D presentation. The sim is flat 2D; world (x, y) maps onto the ground as the 3D
// (x, 0, y) plane, with the 3D Y axis used for height. A fixed tilted camera follows
// the champion for a League-style top-down perspective.
inline constexpr float k_champ_height = 72.0f;
inline constexpr float k_dummy_height = 84.0f;
inline constexpr float k_wall_height = 140.0f;
inline constexpr float k_proj_height = 38.0f;
inline constexpr Vector3 k_cam_offset{0.0f, 1250.0f, 760.0f};
inline constexpr float k_cam_fovy = 55.0f;

inline constexpr Vector3 to3d(const Vector2 p, const float height) {
    return Vector3{p.x, height, p.y};
}
