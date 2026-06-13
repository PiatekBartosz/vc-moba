#include "systems.hpp"

#include "raymath.h"

#include <cstddef>
#include <vector>

#include "config.hpp"

int dummy_at(const World& w, const Vector2 p) {
    for (const Dummy& d : w.dummies) {
        if (Vector2Distance(p, d.pos) <= d.radius) {
            return d.id;
        }
    }
    return -1;
}

static Dummy* find_dummy(World& w, const int id) {
    for (Dummy& d : w.dummies) {
        if (d.id == id) {
            return &d;
        }
    }
    return nullptr;
}

// Nearest living dummy within range of pos, or nullptr.
static Dummy* nearest_dummy_in_range(World& w, const Vector2 pos, const float range) {
    Dummy* best = nullptr;
    float best_dist = range;
    for (Dummy& d : w.dummies) {
        if (d.hp <= 0.0f) {
            continue;
        }
        const float dist = Vector2Distance(pos, d.pos);
        if (dist <= best_dist) {
            best_dist = dist;
            best = &d;
        }
    }
    return best;
}

// Attack-move acquisition (LoL style): among living dummies within attack range of
// the champion, pick the one closest to the cursor/click point.
static Dummy* acquire_toward_cursor(World& w, const Vector2 champ_pos, const Vector2 cursor,
                                    const float range) {
    Dummy* best = nullptr;
    float best_to_cursor = 1e30f;
    for (Dummy& d : w.dummies) {
        if (d.hp <= 0.0f) {
            continue;
        }
        if (Vector2Distance(champ_pos, d.pos) > range) {
            continue;  // must be attackable
        }
        const float to_cursor = Vector2Distance(cursor, d.pos);
        if (to_cursor < best_to_cursor) {
            best_to_cursor = to_cursor;
            best = &d;
        }
    }
    return best;
}

static void spawn_dummy(World& w, const Vector2 pos) {
    w.dummies.push_back(Dummy{w.next_id++, pos});
}

static void spawn_projectile(World& w, const int target_id, const float dmg) {
    Projectile p{};
    p.pos = w.champ.pos;
    p.prev_pos = w.champ.pos;
    p.target_id = target_id;
    p.dmg = dmg;
    w.projectiles.push_back(p);
}

static void apply_damage(World& w, Dummy& d, const float dmg) {
    d.hp = fmaxf(0.0f, d.hp - dmg);
    w.popups.push_back(FloatingText{d.pos, dmg, k_popup_life});
}

static float effective_move_speed(const Champion& c) {
    return c.ghost_buff > 0.0f ? c.move_speed * (1.0f + k_ghost_bonus) : c.move_speed;
}

static void step_toward(Champion& c, const Vector2 target, const float dt) {
    c.pos = Vector2MoveTowards(c.pos, target, effective_move_speed(c) * dt);
}

// Practice reset: refill dummies and clear the champion's transient combat state.
static void reset_world(World& w) {
    for (Dummy& d : w.dummies) {
        d.hp = d.max_hp;
        d.ignite_left = 0.0f;
        d.ignite_dps = 0.0f;
        d.knock_left = 0.0f;
        d.stun_left = 0.0f;
        d.knock_vel = Vector2{0.0f, 0.0f};
    }
    w.projectiles.clear();
    w.popups.clear();

    Champion& c = w.champ;
    c.order = Champion::Order::Idle;
    c.target_id = -1;
    c.windup = 0.0f;
    c.atk_cooldown = 0.0f;
    c.ghost_buff = 0.0f;
    c.dash_left = 0.0f;
    c.dash_vel = Vector2{0.0f, 0.0f};
    c.q_empowered = false;
    c.ult_left = 0.0f;
    c.stealth_left = 0.0f;
    c.sb_target_id = -1;
    c.sb_stacks = 0;
    c.sb_timer = 0.0f;
    c.flash.remaining = 0.0f;
    c.ghost.remaining = 0.0f;
    c.ignite.remaining = 0.0f;
    c.tumble.remaining = 0.0f;
    c.condemn.remaining = 0.0f;
    c.ult.remaining = 0.0f;
}

static void apply_commands(World& w, const Commands& cmds) {
    if (cmds.reset_requested) {
        reset_world(w);
    }
    Champion& c = w.champ;
    if (cmds.move_requested) {
        c.order = Champion::Order::MoveToPoint;
        c.move_point = cmds.move_point;
        c.windup = 0.0f;  // moving before the damage point cancels the wind-up
    }
    if (cmds.attack_requested) {
        c.order = Champion::Order::AttackTarget;
        c.target_id = cmds.attack_target_id;
    }
    if (cmds.attack_move_requested) {
        c.order = Champion::Order::AttackMove;
        c.move_point = cmds.attack_move_point;
    }
    if (cmds.place_requested) {
        spawn_dummy(w, cmds.place_point);
    }
}

// Abilities + cooldown ticking. Runs before movement (spec section 4).
static void abilities_system(World& w, const Commands& cmds, const float dt) {
    Champion& c = w.champ;
    c.flash.tick(dt);
    c.ghost.tick(dt);
    c.ignite.tick(dt);
    c.tumble.tick(dt);
    c.condemn.tick(dt);
    c.ult.tick(dt);
    if (c.ult_left > 0.0f) {
        c.ult_left -= dt;
    }
    if (c.stealth_left > 0.0f) {
        c.stealth_left -= dt;
    }

    if (cmds.toggle_cooldowns) {
        w.cooldowns_enabled = !w.cooldowns_enabled;
    }

    if (cmds.flash_requested && (!w.cooldowns_enabled || c.flash.ready())) {
        // Instant blink toward the cursor, capped at flash range.
        const Vector2 delta = Vector2Subtract(cmds.flash_point, c.pos);
        const float dist = Vector2Length(delta);
        c.pos = dist > k_flash_range
                    ? Vector2Add(c.pos, Vector2Scale(Vector2Normalize(delta), k_flash_range))
                    : cmds.flash_point;
        c.prev_pos = c.pos;  // teleport: don't interpolate a slide across the blink
        if (w.cooldowns_enabled) {
            c.flash.trigger();
        }
    }

    if (cmds.ghost_requested && (!w.cooldowns_enabled || c.ghost.ready())) {
        c.ghost_buff = k_ghost_duration;
        if (w.cooldowns_enabled) {
            c.ghost.trigger();
        }
    }

    if (cmds.ignite_requested && (!w.cooldowns_enabled || c.ignite.ready())) {
        if (Dummy* t = find_dummy(w, cmds.ignite_target_id); t != nullptr) {
            t->ignite_left = k_ignite_duration;
            t->ignite_dps = k_ignite_dps;
            if (w.cooldowns_enabled) {
                c.ignite.trigger();
            }
        }
    }

    // Q - Tumble: a timed roll toward the cursor + auto-attack reset; next auto bonus AD.
    if (cmds.q_requested && (!w.cooldowns_enabled || c.tumble.ready())) {
        Vector2 dir = Vector2Subtract(cmds.q_point, c.pos);
        const float dist = Vector2Length(dir);
        dir = dist > 0.001f ? Vector2Scale(dir, 1.0f / dist) : Vector2{0.0f, 1.0f};
        const float roll_dist = fminf(dist, k_q_dash);
        c.dash_vel = Vector2Scale(dir, roll_dist / k_q_dash_duration);
        c.dash_left = k_q_dash_duration;
        c.atk_cooldown = 0.0f;  // auto-attack reset
        c.windup = 0.0f;
        c.q_empowered = true;
        if (c.ult_left > 0.0f) {
            c.stealth_left = fmaxf(c.stealth_left, k_ult_stealth);  // stealth on Tumble during R
        }
        if (w.cooldowns_enabled) {
            c.tumble.trigger();
        }
    }

    // E - Condemn: knock the current/nearest target back (wall slam resolved later).
    if (cmds.e_requested && (!w.cooldowns_enabled || c.condemn.ready())) {
        Dummy* t = find_dummy(w, c.target_id);
        if (t == nullptr || t->hp <= 0.0f) {
            t = nearest_dummy_in_range(w, c.pos, c.attack_range);
        }
        if (t != nullptr) {
            Vector2 dir = Vector2Subtract(t->pos, c.pos);
            if (Vector2Length(dir) < 0.001f) {
                dir = Vector2{0.0f, 1.0f};
            }
            t->knock_vel = Vector2Scale(Vector2Normalize(dir), k_condemn_speed);
            t->knock_left = k_condemn_knockback / k_condemn_speed;
            if (w.cooldowns_enabled) {
                c.condemn.trigger();
            }
        }
    }

    // R - Final Hour: bonus AD for a duration + brief stealth on cast.
    if (cmds.ult_requested && (!w.cooldowns_enabled || c.ult.ready())) {
        c.ult_left = k_ult_duration;
        c.stealth_left = k_ult_stealth;
        if (w.cooldowns_enabled) {
            c.ult.trigger();
        }
    }
}

// Tumble roll: drives the champion along the dash velocity for its duration.
static void dash_system(Champion& c, const float dt) {
    if (c.dash_left <= 0.0f) {
        return;
    }
    c.pos = Vector2Add(c.pos, Vector2Scale(c.dash_vel, dt));
    c.dash_left -= dt;
}

static void movement_system(Champion& c, const float dt) {
    if (c.dash_left > 0.0f) {
        return;  // rolling overrides ordinary movement
    }
    if (c.order != Champion::Order::MoveToPoint) {
        return;
    }
    step_toward(c, c.move_point, dt);
    if (Vector2Distance(c.pos, c.move_point) <= k_arrive_epsilon) {
        c.order = Champion::Order::Idle;
    }
}

// Auto-attack state machine (chase -> windup -> damage point). See spec section 6.
// Handles explicit AttackTarget and attack-move (auto-acquire nearby dummies).
static void combat_system(World& w, const float dt) {
    Champion& c = w.champ;
    c.atk_cooldown -= dt;

    if (c.windup > 0.0f) {  // committed; locked in place until the damage point
        c.windup -= dt;
        if (c.windup <= 0.0f) {
            float dmg = c.atk_damage;
            if (c.ult_left > 0.0f) {
                dmg += k_ult_bonus_ad;  // Final Hour bonus AD
            }
            if (c.q_empowered) {
                dmg += k_q_bonus_ad;  // Tumble empowered auto
                c.q_empowered = false;
            }
            spawn_projectile(w, c.target_id, dmg);  // damage point
        }
        return;
    }

    if (c.dash_left > 0.0f) {
        return;  // rolling: don't chase or start a new attack mid-roll
    }

    Dummy* t = nullptr;
    if (c.order == Champion::Order::AttackTarget) {
        t = find_dummy(w, c.target_id);
        if (t == nullptr || t->hp <= 0.0f) {  // target gone/dead -> stop
            c.order = Champion::Order::Idle;
            return;
        }
    } else if (c.order == Champion::Order::AttackMove) {
        t = acquire_toward_cursor(w, c.pos, c.move_point, c.attack_range);
        c.target_id = t != nullptr ? t->id : -1;
    } else {
        return;
    }

    if (t != nullptr) {
        const float d = Vector2Distance(c.pos, t->pos);
        if (d > c.attack_range) {
            step_toward(c, t->pos, dt);  // chase into range
        } else if (c.atk_cooldown <= 0.0f) {
            c.windup = k_windup_fraction / c.attacks_per_sec;  // begin the attack
            c.atk_cooldown = 1.0f / c.attacks_per_sec;
        }
    } else {
        // Attack-move with nothing in range: keep advancing toward the destination.
        step_toward(c, c.move_point, dt);
        if (Vector2Distance(c.pos, c.move_point) <= k_arrive_epsilon) {
            c.order = Champion::Order::Idle;
        }
    }
}

// Push the champion circle out of any wall it overlaps. Resolving perpendicular to
// the nearest wall face preserves tangential motion, so the champion slides.
static void collision_system(World& w) {
    Champion& c = w.champ;
    const float r = k_champ_radius;
    for (int pass = 0; pass < 2; ++pass) {  // a couple of passes settle corners
        for (const Rectangle& wall : w.walls) {
            const Vector2 closest{Clamp(c.pos.x, wall.x, wall.x + wall.width),
                                  Clamp(c.pos.y, wall.y, wall.y + wall.height)};
            const Vector2 delta = Vector2Subtract(c.pos, closest);
            const float dist = Vector2Length(delta);
            if (dist >= r) {
                continue;
            }
            if (dist > 0.001f) {
                c.pos = Vector2Add(closest, Vector2Scale(Vector2Normalize(delta), r));
            } else {
                // Center is inside the rect: eject along the shallowest axis.
                const float left = c.pos.x - wall.x;
                const float right = (wall.x + wall.width) - c.pos.x;
                const float top = c.pos.y - wall.y;
                const float bottom = (wall.y + wall.height) - c.pos.y;
                const float m = fminf(fminf(left, right), fminf(top, bottom));
                if (m == left) {
                    c.pos.x = wall.x - r;
                } else if (m == right) {
                    c.pos.x = wall.x + wall.width + r;
                } else if (m == top) {
                    c.pos.y = wall.y - r;
                } else {
                    c.pos.y = wall.y + wall.height + r;
                }
            }
        }
    }
}

// Condemn knockback: shove a target along its knock velocity each step; a wall slam
// stuns it and deals bonus damage, then stops the knockback.
static void knockback_system(World& w, const float dt) {
    for (Dummy& d : w.dummies) {
        if (d.stun_left > 0.0f) {
            d.stun_left -= dt;
        }
        if (d.knock_left <= 0.0f) {
            continue;
        }
        d.pos = Vector2Add(d.pos, Vector2Scale(d.knock_vel, dt));
        d.knock_left -= dt;
        for (const Rectangle& wall : w.walls) {
            const Vector2 closest{Clamp(d.pos.x, wall.x, wall.x + wall.width),
                                  Clamp(d.pos.y, wall.y, wall.y + wall.height)};
            const Vector2 delta = Vector2Subtract(d.pos, closest);
            if (Vector2Length(delta) < d.radius) {
                apply_damage(w, d, k_condemn_wall_damage);
                d.stun_left = k_condemn_stun;
                d.knock_left = 0.0f;
                if (Vector2Length(delta) > 0.001f) {
                    d.pos = Vector2Add(closest, Vector2Scale(Vector2Normalize(delta), d.radius));
                }
                break;
            }
        }
    }
}

// Homing projectiles advance toward their target and apply damage on contact.
static void projectile_system(World& w, const float dt) {
    for (Projectile& p : w.projectiles) {
        p.prev_pos = p.pos;
        Dummy* t = find_dummy(w, p.target_id);
        if (t == nullptr) {
            p.live = false;
            continue;
        }
        p.pos = Vector2MoveTowards(p.pos, t->pos, p.speed * dt);
        if (Vector2Distance(p.pos, t->pos) < k_hit_distance) {
            apply_damage(w, *t, p.dmg);
            // W - Silver Bolts: every 3rd consecutive hit on a target adds % max-HP true dmg.
            Champion& c = w.champ;
            if (c.sb_target_id == t->id) {
                c.sb_stacks += 1;
            } else {
                c.sb_target_id = t->id;  // stacks live on a single target
                c.sb_stacks = 1;
            }
            c.sb_timer = k_silverbolts_duration;  // refresh the stack timer
            if (c.sb_stacks >= k_silverbolts_stacks) {
                apply_damage(w, *t, t->max_hp * k_silverbolts_percent);
                c.sb_stacks = 0;
            }
            p.live = false;
        }
    }
    std::erase_if(w.projectiles, [](const Projectile& p) { return !p.live; });
}

// Buff expiry + floating damage numbers drift up and fade, then get cleaned up.
static void effects_system(World& w, const float dt) {
    if (w.champ.ghost_buff > 0.0f) {
        w.champ.ghost_buff -= dt;
    }
    if (w.champ.sb_timer > 0.0f) {  // Silver Bolts stacks decay if the target isn't hit
        w.champ.sb_timer -= dt;
        if (w.champ.sb_timer <= 0.0f) {
            w.champ.sb_stacks = 0;
            w.champ.sb_target_id = -1;
        }
    }
    for (Dummy& d : w.dummies) {
        if (d.ignite_left > 0.0f) {
            d.hp = fmaxf(0.0f, d.hp - d.ignite_dps * dt);
            d.ignite_left -= dt;
        }
    }
    for (FloatingText& f : w.popups) {
        f.life -= dt;  // rise is applied in screen space at render time
    }
    std::erase_if(w.popups, [](const FloatingText& f) { return f.life <= 0.0f; });
}

// --- Survival ("Normal") mode -------------------------------------------------

static Dummy* nearest_hostile(World& w, const Vector2 pos, const float range) {
    Dummy* best = nullptr;
    float best_dist = range;
    for (Dummy& d : w.dummies) {
        if (!d.hostile || d.hp <= 0.0f) {
            continue;
        }
        const float dist = Vector2Distance(pos, d.pos);
        if (dist <= best_dist) {
            best_dist = dist;
            best = &d;
        }
    }
    return best;
}

// While idle (and not mid-roll/attack), auto-acquire the nearest enemy (VS-style).
static void auto_engage(World& w) {
    Champion& c = w.champ;
    if (c.dash_left > 0.0f || c.windup > 0.0f || c.order != Champion::Order::Idle) {
        return;
    }
    if (Dummy* t = nearest_hostile(w, c.pos, c.attack_range * 1.5f); t != nullptr) {
        c.order = Champion::Order::AttackTarget;
        c.target_id = t->id;
    }
}

static void spawn_enemy(World& w, const Vector2 pos) {
    Dummy e{};
    e.id = w.next_id++;
    e.pos = pos;
    e.radius = k_enemy_radius;
    e.max_hp = k_enemy_hp + w.game_time * 1.6f;  // tougher over time
    e.hp = e.max_hp;
    e.hostile = true;
    e.speed = k_enemy_speed;
    e.touch_dps = k_enemy_touch_dps;
    w.dummies.push_back(e);
}

// Spawn waves of enemies around the champion, ramping in count and rate over time.
static void spawn_system(World& w, const float dt) {
    w.game_time += dt;
    w.spawn_timer -= dt;
    if (w.spawn_timer > 0.0f) {
        return;
    }
    w.spawn_timer = fmaxf(k_spawn_interval_min, k_spawn_interval_base - w.game_time * 0.02f);
    const int count = 1 + static_cast<int>(w.game_time / 18.0f);
    for (int i = 0; i < count; ++i) {
        const float ang = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
        spawn_enemy(w, Vector2{w.champ.pos.x + cosf(ang) * k_spawn_radius,
                               w.champ.pos.y + sinf(ang) * k_spawn_radius});
    }
}

// Enemies chase the champion and deal contact damage (knocked/stunned enemies hold).
static void enemy_system(World& w, const float dt) {
    Champion& c = w.champ;
    for (Dummy& d : w.dummies) {
        if (!d.hostile || d.hp <= 0.0f || d.knock_left > 0.0f || d.stun_left > 0.0f) {
            continue;
        }
        d.pos = Vector2MoveTowards(d.pos, c.pos, d.speed * dt);
        if (Vector2Distance(d.pos, c.pos) <= d.radius + k_champ_radius) {
            c.hp -= d.touch_dps * dt;
        }
    }
    if (c.hp < 0.0f) {
        c.hp = 0.0f;
    }
}

// Periodically drop health kits; picking one up heals the champion.
static void kit_system(World& w, const float dt) {
    w.kit_timer -= dt;
    if (w.kit_timer <= 0.0f) {
        w.kit_timer = k_kit_interval;
        const float ang = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
        const float r = static_cast<float>(GetRandomValue(300, 900));
        w.kits.push_back(HealthKit{
            Vector2{w.champ.pos.x + cosf(ang) * r, w.champ.pos.y + sinf(ang) * r}, k_kit_heal});
    }
    for (std::size_t i = 0; i < w.kits.size();) {
        if (Vector2Distance(w.champ.pos, w.kits[i].pos) <= k_kit_radius + k_champ_radius) {
            w.champ.hp = fminf(w.champ.max_hp, w.champ.hp + w.kits[i].heal);
            w.kits.erase(w.kits.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void start_survival(World& w) {
    std::erase_if(w.dummies, [](const Dummy& d) { return d.hostile; });
    w.kits.clear();
    w.projectiles.clear();
    w.popups.clear();
    w.game_time = 0.0f;
    w.spawn_timer = 0.0f;
    w.kit_timer = k_kit_interval;
    w.kills = 0;

    Champion& c = w.champ;
    c.pos = Vector2{0.0f, 0.0f};
    c.prev_pos = c.pos;
    c.hp = c.max_hp;
    c.mana = c.max_mana;
    c.order = Champion::Order::Idle;
    c.target_id = -1;
    c.windup = 0.0f;
    c.atk_cooldown = 0.0f;
    c.dash_left = 0.0f;
    c.ghost_buff = 0.0f;
    c.ult_left = 0.0f;
    c.stealth_left = 0.0f;
    c.q_empowered = false;
    c.sb_stacks = 0;
    c.sb_target_id = -1;
    c.sb_timer = 0.0f;
}

void start_practice(World& w) {
    std::erase_if(w.dummies, [](const Dummy& d) { return d.hostile; });
    w.kits.clear();
    w.champ.hp = w.champ.max_hp;
    w.champ.mana = w.champ.max_mana;
}

// Remove dead enemies (counting kills); restart the run if the champion dies.
static void survival_cleanup(World& w) {
    std::erase_if(w.dummies, [&w](const Dummy& d) {
        if (d.hostile && d.hp <= 0.0f) {
            w.kills += 1;
            return true;
        }
        return false;
    });
    if (w.champ.hp <= 0.0f) {
        start_survival(w);  // died -> restart the survival run
    }
}

void update(World& w, const Commands& cmds, const float dt) {
    w.champ.prev_pos = w.champ.pos;

    apply_commands(w, cmds);
    abilities_system(w, cmds, dt);
    if (w.survival) {
        auto_engage(w);
    }
    dash_system(w.champ, dt);
    movement_system(w.champ, dt);
    combat_system(w, dt);
    collision_system(w);
    knockback_system(w, dt);
    if (w.survival) {
        spawn_system(w, dt);
        enemy_system(w, dt);
        kit_system(w, dt);
    }
    projectile_system(w, dt);
    effects_system(w, dt);
    if (w.survival) {
        survival_cleanup(w);
    }
}
