#pragma once

#include "raylib.h"

#include <vector>

#include "config.hpp"

// Game data. Behavior lives in the systems (see systems.hpp); these are plain data.

struct Cooldown {
    float remaining = 0.0f;
    float total = 0.0f;

    bool ready() const { return remaining <= 0.0f; }
    void trigger() { remaining = total; }
    void tick(const float dt) {
        if (remaining > 0.0f) {
            remaining -= dt;
        }
    }
};

struct Champion {
    Vector2 pos{0.0f, 0.0f};
    Vector2 prev_pos{0.0f, 0.0f};  // sim position one step ago, for render interpolation
    float move_speed = k_move_speed;

    float hp = k_champ_hp;
    float max_hp = k_champ_hp;
    float mana = k_champ_mana;  // placeholder resource for now
    float max_mana = k_champ_mana;

    float attack_range = k_attack_range;
    float attacks_per_sec = k_attacks_per_sec;
    float atk_damage = k_attack_damage;

    enum class Order { Idle, MoveToPoint, AttackTarget, AttackMove };
    Order order = Order::Idle;
    Vector2 move_point{0.0f, 0.0f};
    int target_id = -1;

    float atk_cooldown = 0.0f;  // time until the next attack is allowed
    float windup = 0.0f;        // >0 == mid-attack; damage point is when it reaches 0
    float ghost_buff = 0.0f;    // seconds of move-speed buff remaining

    // Vayne kit state.
    float dash_left = 0.0f;     // Tumble roll time remaining (>0 == mid-roll)
    Vector2 dash_vel{0.0f, 0.0f};
    bool q_empowered = false;  // next auto deals Tumble bonus AD
    float ult_left = 0.0f;     // Final Hour duration remaining (bonus AD while > 0)
    float stealth_left = 0.0f;  // invisibility remaining
    int sb_target_id = -1;     // Silver Bolts: the single stacked target
    int sb_stacks = 0;         // Silver Bolts: consecutive hits on that target
    float sb_timer = 0.0f;     // Silver Bolts: time until stacks expire

    Cooldown flash{0.0f, k_flash_cd};
    Cooldown ghost{0.0f, k_ghost_cd};
    Cooldown ignite{0.0f, k_ignite_cd};
    Cooldown tumble{0.0f, k_q_cd};
    Cooldown condemn{0.0f, k_condemn_cd};
    Cooldown ult{0.0f, k_ult_cd};
};

struct Dummy {
    int id = 0;
    Vector2 pos{0.0f, 0.0f};
    float radius = k_dummy_radius;
    float hp = k_dummy_hp;
    float max_hp = k_dummy_hp;
    float ignite_left = 0.0f;  // seconds of ignite DoT remaining
    float ignite_dps = 0.0f;
    Vector2 knock_vel{0.0f, 0.0f};  // Condemn knockback velocity
    float knock_left = 0.0f;        // knockback time remaining
    float stun_left = 0.0f;         // stun remaining

    // Survival-mode enemy behaviour (practice dummies leave these at defaults).
    bool hostile = false;       // chases the champion and deals contact damage
    float speed = 0.0f;         // enemy move speed
    float touch_dps = 0.0f;     // contact damage per second to the champion
};

struct HealthKit {
    Vector2 pos{0.0f, 0.0f};
    float heal = k_kit_heal;
};

struct Projectile {
    Vector2 pos{0.0f, 0.0f};
    Vector2 prev_pos{0.0f, 0.0f};  // for render interpolation
    int target_id = -1;
    float speed = k_projectile_speed;
    float dmg = 0.0f;
    bool live = true;
};

struct FloatingText {
    Vector2 pos{0.0f, 0.0f};
    float value = 0.0f;
    float life = k_popup_life;
};

struct World {
    Champion champ;
    std::vector<Dummy> dummies;
    std::vector<Projectile> projectiles;
    std::vector<FloatingText> popups;
    std::vector<Rectangle> walls;
    std::vector<HealthKit> kits;
    bool cooldowns_enabled = true;  // practice toggle (false == abilities always ready)
    int next_id = 0;

    // Survival-mode run state.
    bool survival = false;
    float game_time = 0.0f;
    float spawn_timer = 0.0f;
    float kit_timer = 0.0f;
    int kills = 0;
};
