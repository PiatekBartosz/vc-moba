#include "systems.hpp"

#include "raymath.h"

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
    }
    w.projectiles.clear();
    w.popups.clear();

    Champion& c = w.champ;
    c.order = Champion::Order::Idle;
    c.target_id = -1;
    c.windup = 0.0f;
    c.atk_cooldown = 0.0f;
    c.ghost_buff = 0.0f;
    c.flash.remaining = 0.0f;
    c.ghost.remaining = 0.0f;
    c.ignite.remaining = 0.0f;
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
}

static void movement_system(Champion& c, const float dt) {
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
            spawn_projectile(w, c.target_id, c.atk_damage);  // damage point
        }
        return;
    }

    Dummy* t = nullptr;
    if (c.order == Champion::Order::AttackTarget) {
        t = find_dummy(w, c.target_id);
        if (t == nullptr || t->hp <= 0.0f) {  // target gone/dead -> stop
            c.order = Champion::Order::Idle;
            return;
        }
    } else if (c.order == Champion::Order::AttackMove) {
        t = nearest_dummy_in_range(w, c.pos, c.attack_range);
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

void update(World& w, const Commands& cmds, const float dt) {
    w.champ.prev_pos = w.champ.pos;

    apply_commands(w, cmds);
    abilities_system(w, cmds, dt);
    movement_system(w.champ, dt);
    combat_system(w, dt);
    collision_system(w);
    projectile_system(w, dt);
    effects_system(w, dt);
}
