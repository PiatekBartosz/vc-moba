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
inline constexpr float k_ignite_dps = 50.0f;  // ignite kept in code but currently unbound
inline constexpr float k_ignite_duration = 5.0f;
inline constexpr float k_ignite_cd = 180.0f;

// Vayne kit ------------------------------------------------------------------
// Q - Tumble: short dash + auto-attack reset, next auto deals bonus AD.
inline constexpr float k_q_dash = 320.0f;
inline constexpr float k_q_dash_duration = 0.27f;  // roll travel time
inline constexpr float k_q_cd = 5.0f;
inline constexpr float k_q_bonus_ad = 55.0f;
// W - Silver Bolts: every 3rd consecutive hit on a target deals % max-HP true dmg.
inline constexpr int k_silverbolts_stacks = 3;
inline constexpr float k_silverbolts_percent = 0.08f;
inline constexpr float k_silverbolts_duration = 4.0f;  // stacks expire if the target isn't hit
// E - Condemn: knock the target back; stun + bonus damage if it hits a wall.
inline constexpr float k_condemn_cd = 16.0f;
inline constexpr float k_condemn_knockback = 330.0f;
inline constexpr float k_condemn_speed = 1500.0f;
inline constexpr float k_condemn_wall_damage = 130.0f;
inline constexpr float k_condemn_stun = 1.6f;
// R - Final Hour: bonus AD for a duration; stealth on cast / on Tumble during it.
inline constexpr float k_ult_cd = 70.0f;
inline constexpr float k_ult_duration = 8.0f;
inline constexpr float k_ult_bonus_ad = 45.0f;
inline constexpr float k_ult_stealth = 1.5f;

inline constexpr float k_projectile_speed = 1800.0f;
inline constexpr float k_projectile_radius = 7.0f;
inline constexpr float k_hit_distance = 6.0f;

inline constexpr float k_popup_life = 0.8f;
inline constexpr float k_popup_rise = 70.0f;  // upward drift, units/sec

inline constexpr float k_dummy_radius = 28.0f;
inline constexpr float k_dummy_hp = 1000.0f;

// Champion vitals (mana is a placeholder for now).
inline constexpr float k_champ_hp = 680.0f;
inline constexpr float k_champ_mana = 320.0f;

// Survival ("Normal") mode -------------------------------------------------
inline constexpr float k_enemy_radius = 26.0f;
inline constexpr float k_enemy_height = 64.0f;
inline constexpr float k_enemy_hp = 110.0f;
inline constexpr float k_enemy_speed = 155.0f;
inline constexpr float k_enemy_touch_dps = 24.0f;
inline constexpr float k_spawn_radius = 1500.0f;       // spawn distance from the champion
inline constexpr float k_spawn_interval_base = 2.2f;   // seconds between waves at t = 0
inline constexpr float k_spawn_interval_min = 0.45f;
inline constexpr float k_kit_interval = 9.0f;          // seconds between health-kit spawns
inline constexpr float k_kit_heal = 200.0f;
inline constexpr float k_kit_radius = 28.0f;

// 3D presentation. The sim is flat 2D; world (x, y) maps onto the ground as the 3D
// (x, 0, y) plane, with the 3D Y axis used for height. A fixed tilted camera follows
// the champion for a League-style top-down perspective.
inline constexpr float k_champ_model_scale = 1.8f;  // visual only; hitbox stays k_champ_radius
inline constexpr float k_champ_height = 72.0f;
inline constexpr float k_dummy_height = 84.0f;
inline constexpr float k_wall_height = 140.0f;
inline constexpr float k_proj_height = 38.0f;
inline constexpr Vector3 k_cam_offset{0.0f, 1250.0f, 760.0f};
inline constexpr float k_cam_fovy = 55.0f;

inline constexpr Vector3 to3d(const Vector2 p, const float height) {
    return Vector3{p.x, height, p.y};
}
