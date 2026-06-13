#include "raylib.h"
#include "raymath.h"

#include <vector>

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
inline constexpr float k_ghost_cd = 210.0f;
inline constexpr float k_ignite_cd = 180.0f;

inline constexpr float k_projectile_speed = 1800.0f;
inline constexpr float k_projectile_radius = 7.0f;
inline constexpr float k_hit_distance = 6.0f;

inline constexpr float k_popup_life = 0.8f;
inline constexpr float k_popup_rise = 70.0f;  // upward drift, units/sec

inline constexpr float k_dummy_radius = 28.0f;
inline constexpr float k_dummy_hp = 1000.0f;

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

    float attack_range = k_attack_range;
    float attacks_per_sec = k_attacks_per_sec;
    float atk_damage = k_attack_damage;

    enum class Order { Idle, MoveToPoint, AttackTarget };
    Order order = Order::Idle;
    Vector2 move_point{0.0f, 0.0f};
    int target_id = -1;

    float atk_cooldown = 0.0f;  // time until the next attack is allowed
    float windup = 0.0f;        // >0 == mid-attack; damage point is when it reaches 0

    Cooldown flash{0.0f, k_flash_cd};
    Cooldown ghost{0.0f, k_ghost_cd};
    Cooldown ignite{0.0f, k_ignite_cd};
};

struct Dummy {
    int id = 0;
    Vector2 pos{0.0f, 0.0f};
    float radius = k_dummy_radius;
    float hp = k_dummy_hp;
    float max_hp = k_dummy_hp;
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
    bool cooldowns_enabled = true;  // practice toggle (false == abilities always ready)
    int next_id = 0;
};

struct Commands {
    bool move_requested = false;
    Vector2 move_point{0.0f, 0.0f};

    bool attack_requested = false;
    int attack_target_id = -1;

    bool place_requested = false;
    Vector2 place_point{0.0f, 0.0f};

    bool flash_requested = false;
    Vector2 flash_point{0.0f, 0.0f};

    bool toggle_cooldowns = false;

    // Order latches (move/attack) are re-issued each frame and cleared after the
    // substep loop. One-shots are consumed after each substep so they fire once.
    void clear_orders() {
        move_requested = false;
        attack_requested = false;
    }
    void consume_one_shots() {
        place_requested = false;
        flash_requested = false;
        toggle_cooldowns = false;
    }
};

static int dummy_at(const World& w, const Vector2 p) {
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

static void poll_input(const World& w, Commands& cmds, const Camera2D& camera) {
    // Hold-to-move (LoL style): while the right button is held, keep re-issuing the
    // order toward the cursor. Right-click on a dummy attacks it; on ground, moves.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 wp = GetScreenToWorld2D(GetMousePosition(), camera);
        const int hit = dummy_at(w, wp);
        if (hit >= 0) {
            cmds.attack_requested = true;
            cmds.attack_target_id = hit;
        } else {
            cmds.move_requested = true;
            cmds.move_point = wp;
        }
    }
    if (IsKeyPressed(KEY_T)) {
        cmds.place_requested = true;
        cmds.place_point = GetScreenToWorld2D(GetMousePosition(), camera);
    }
    if (IsKeyPressed(KEY_D)) {
        cmds.flash_requested = true;
        cmds.flash_point = GetScreenToWorld2D(GetMousePosition(), camera);
    }
    if (IsKeyPressed(KEY_N)) {
        cmds.toggle_cooldowns = true;
    }
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

static void step_toward(Champion& c, const Vector2 target, const float dt) {
    c.pos = Vector2MoveTowards(c.pos, target, c.move_speed * dt);
}

static void apply_commands(World& w, const Commands& cmds) {
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

    if (c.order != Champion::Order::AttackTarget) {
        return;
    }
    Dummy* t = find_dummy(w, c.target_id);
    if (t == nullptr) {
        c.order = Champion::Order::Idle;
        return;
    }

    const float d = Vector2Distance(c.pos, t->pos);
    if (d > c.attack_range) {
        step_toward(c, t->pos, dt);  // chase into range
    } else if (c.atk_cooldown <= 0.0f) {
        c.windup = k_windup_fraction / c.attacks_per_sec;  // begin the attack
        c.atk_cooldown = 1.0f / c.attacks_per_sec;
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

// Floating damage numbers drift up and fade, then get cleaned up.
static void effects_system(World& w, const float dt) {
    for (FloatingText& f : w.popups) {
        f.pos.y -= k_popup_rise * dt;
        f.life -= dt;
    }
    std::erase_if(w.popups, [](const FloatingText& f) { return f.life <= 0.0f; });
}

static void update(World& w, const Commands& cmds, const float dt) {
    w.champ.prev_pos = w.champ.pos;

    apply_commands(w, cmds);
    abilities_system(w, cmds, dt);
    movement_system(w.champ, dt);
    combat_system(w, dt);
    projectile_system(w, dt);
    effects_system(w, dt);
}

static void draw_grid() {
    const Color line_color{40, 40, 48, 255};
    const Color axis_color{70, 70, 82, 255};

    for (float x = -k_arena_half_extent; x <= k_arena_half_extent; x += k_grid_spacing) {
        DrawLineV(Vector2{x, -k_arena_half_extent}, Vector2{x, k_arena_half_extent}, line_color);
    }
    for (float y = -k_arena_half_extent; y <= k_arena_half_extent; y += k_grid_spacing) {
        DrawLineV(Vector2{-k_arena_half_extent, y}, Vector2{k_arena_half_extent, y}, line_color);
    }

    DrawLineV(Vector2{0.0f, -k_arena_half_extent}, Vector2{0.0f, k_arena_half_extent}, axis_color);
    DrawLineV(Vector2{-k_arena_half_extent, 0.0f}, Vector2{k_arena_half_extent, 0.0f}, axis_color);
}

static void draw_health_bar(const Vector2 center, const float radius, const float hp,
                            const float max_hp) {
    const float bw = 56.0f;
    const float bh = 7.0f;
    const float x = center.x - bw * 0.5f;
    const float y = center.y - radius - 16.0f;
    const float frac = max_hp > 0.0f ? Clamp(hp / max_hp, 0.0f, 1.0f) : 0.0f;

    DrawRectangleRec(Rectangle{x - 1.0f, y - 1.0f, bw + 2.0f, bh + 2.0f}, Color{0, 0, 0, 180});
    DrawRectangleRec(Rectangle{x, y, bw, bh}, Color{60, 20, 20, 255});
    DrawRectangleRec(Rectangle{x, y, bw * frac, bh}, Color{210, 60, 60, 255});
}

static void draw_dummy(const Dummy& d) {
    DrawCircleV(d.pos, d.radius, Color{170, 110, 90, 255});
    DrawCircleLinesV(d.pos, d.radius, Color{230, 180, 160, 255});
    draw_health_bar(d.pos, d.radius, d.hp, d.max_hp);
}

// Fill a convex polygon by fanning from p[0]. Each triangle is drawn in both
// windings; backface culling keeps exactly one, so the fill is correct regardless
// of vertex order and works inside raylib's batched 2D pipeline.
static void fill_poly(const Vector2* p, const int n, const Color c) {
    for (int i = 1; i + 1 < n; ++i) {
        DrawTriangle(p[0], p[i], p[i + 1], c);
        DrawTriangle(p[0], p[i + 1], p[i], c);
    }
}

static void draw_cursor() {
    const Vector2 m = GetMousePosition();
    const bool moving = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    constexpr float scale = 1.9f;

    // Classic pointer arrow, tip at (0,0) == the mouse hotspot. Outline order;
    // the fill is split into the head triangle + the tail leg (both convex).
    static const Vector2 base[7] = {
        {0.0f, 0.0f}, {0.0f, 24.0f}, {5.5f, 18.5f}, {9.0f, 27.0f},
        {12.5f, 25.5f}, {9.0f, 17.0f}, {16.0f, 17.0f},
    };

    Vector2 pts[7];
    Vector2 shadow[7];
    for (int i = 0; i < 7; ++i) {
        pts[i] = Vector2{m.x + base[i].x * scale, m.y + base[i].y * scale};
        shadow[i] = Vector2{pts[i].x + 3.0f, pts[i].y + 4.0f};
    }

    const Color fill = moving ? Color{152, 236, 162, 255} : Color{234, 239, 248, 255};
    const Color outline = Color{22, 26, 42, 255};
    const Color accent = moving ? Color{70, 170, 82, 255} : Color{214, 172, 76, 255};

    const Vector2 head[3] = {pts[0], pts[1], pts[6]};
    const Vector2 leg[4] = {pts[2], pts[3], pts[4], pts[5]};
    const Vector2 shead[3] = {shadow[0], shadow[1], shadow[6]};
    const Vector2 sleg[4] = {shadow[2], shadow[3], shadow[4], shadow[5]};

    // soft drop shadow
    fill_poly(shead, 3, Color{0, 0, 0, 80});
    fill_poly(sleg, 4, Color{0, 0, 0, 80});

    // body
    fill_poly(head, 3, fill);
    fill_poly(leg, 4, fill);

    // dark outline around the full arrow silhouette
    for (int i = 0; i < 7; ++i) {
        DrawLineEx(pts[i], pts[(i + 1) % 7], 2.5f, outline);
    }

    // gold/green highlight streak down the leading edge
    const Vector2 streak_end{pts[0].x + (pts[1].x - pts[0].x) * 0.6f,
                             pts[0].y + (pts[1].y - pts[0].y) * 0.6f};
    DrawLineEx(pts[0], streak_end, 2.5f, accent);
}

static void draw_ability_slot(const float x, const float y, const float size, const char* key,
                              const char* name, const Cooldown& cd, const bool practice) {
    const Rectangle box{x, y, size, size};
    const bool ready = practice || cd.ready();

    DrawRectangleRec(box, Color{28, 32, 42, 235});
    DrawRectangleLinesEx(box, 2.0f, ready ? Color{214, 196, 110, 255} : Color{70, 76, 92, 255});

    const Vector2 center{x + size * 0.5f, y + size * 0.5f};

    if (!practice && cd.remaining > 0.0f && cd.total > 0.0f) {
        const float frac = Clamp(cd.remaining / cd.total, 0.0f, 1.0f);
        DrawCircleSector(center, size * 0.5f, -90.0f, -90.0f + 360.0f * frac, 48,
                         Color{0, 0, 0, 150});
        const char* num = TextFormat("%.0f", static_cast<double>(ceilf(cd.remaining)));
        const int nw = MeasureText(num, 24);
        DrawText(num, static_cast<int>(center.x) - nw / 2, static_cast<int>(center.y) - 12, 24,
                 RAYWHITE);
    } else {
        const int kw = MeasureText(key, 28);
        DrawText(key, static_cast<int>(center.x) - kw / 2, static_cast<int>(center.y) - 14, 28,
                 ready ? RAYWHITE : Color{150, 156, 170, 255});
    }

    const int label_w = MeasureText(name, 16);
    DrawText(name, static_cast<int>(center.x) - label_w / 2, static_cast<int>(y + size) + 4, 16,
             Color{180, 186, 200, 255});
}

static void draw_ability_bar(const World& w) {
    const Champion& c = w.champ;
    const float size = 64.0f;
    const float gap = 12.0f;
    const float total_w = size * 3.0f + gap * 2.0f;
    const float x0 = (static_cast<float>(k_window_size_x) - total_w) * 0.5f;
    const float y = static_cast<float>(k_window_size_y) - size - 28.0f;
    const bool practice = !w.cooldowns_enabled;

    draw_ability_slot(x0, y, size, "D", "Flash", c.flash, practice);
    draw_ability_slot(x0 + (size + gap), y, size, "F", "Ghost", c.ghost, practice);
    draw_ability_slot(x0 + (size + gap) * 2.0f, y, size, "R", "Ignite", c.ignite, practice);

    if (practice) {
        const char* t = "NO COOLDOWNS (N)";
        const int tw = MeasureText(t, 20);
        DrawText(t, static_cast<int>(x0 + total_w * 0.5f) - tw / 2, static_cast<int>(y) - 28, 20,
                 Color{120, 220, 140, 255});
    }
}

static void render(const World& w, const Vector2 champ_pos, const float alpha,
                   const Camera2D& camera) {
    BeginDrawing();
    ClearBackground(Color{18, 18, 22, 255});

    BeginMode2D(camera);
    draw_grid();
    for (const Dummy& d : w.dummies) {
        draw_dummy(d);
    }

    // Attack-range indicator around the champion.
    DrawCircleLinesV(champ_pos, w.champ.attack_range, Color{80, 110, 150, 110});

    // Highlight the current attack target.
    if (w.champ.order == Champion::Order::AttackTarget) {
        for (const Dummy& d : w.dummies) {
            if (d.id == w.champ.target_id) {
                DrawCircleLinesV(d.pos, d.radius + 6.0f, Color{240, 210, 80, 255});
                break;
            }
        }
    }
    if (w.champ.order == Champion::Order::MoveToPoint) {
        DrawCircleV(w.champ.move_point, 5.0f, Color{120, 200, 120, 255});
    }

    // Champion turns gold while winding up an attack (locked in place).
    const Color champ_fill =
        w.champ.windup > 0.0f ? Color{230, 200, 90, 255} : Color{90, 150, 240, 255};
    DrawCircleV(champ_pos, k_champ_radius, champ_fill);
    DrawCircleLinesV(champ_pos, k_champ_radius, Color{200, 220, 255, 255});

    // Projectiles (interpolated for smoothness at high frame rates).
    for (const Projectile& p : w.projectiles) {
        const Vector2 pp = Vector2Lerp(p.prev_pos, p.pos, alpha);
        DrawCircleV(pp, k_projectile_radius, Color{250, 230, 140, 255});
        DrawCircleLinesV(pp, k_projectile_radius, Color{255, 255, 220, 255});
    }

    // Floating damage numbers.
    for (const FloatingText& f : w.popups) {
        const unsigned char a = static_cast<unsigned char>(Clamp(f.life / k_popup_life, 0.0f, 1.0f) * 255.0f);
        DrawText(TextFormat("%.0f", static_cast<double>(f.value)), static_cast<int>(f.pos.x) - 14,
                 static_cast<int>(f.pos.y) - 40, 24, Color{255, 240, 160, a});
    }
    EndMode2D();

    draw_ability_bar(w);

    draw_cursor();
    DrawFPS(10, 10);
    DrawText("Right-click: move / attack    T: dummy    D: flash    N: no-cooldowns", 10, 35, 20,
             Color{200, 200, 210, 255});
    EndDrawing();
}

int main() {
    InitWindow(k_window_size_x, k_window_size_y, "Practice Tool");
    SetTargetFPS(k_target_fps);
    HideCursor();  // we draw our own cursor in draw_cursor()

    World world{};

    Camera2D camera{};
    camera.offset = Vector2{k_window_size_x / 2.0f, k_window_size_y / 2.0f};
    camera.target = world.champ.pos;
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    Commands cmds{};
    float acc = 0.0f;
    while (!WindowShouldClose()) {
        // Clamp to avoid a spiral of death if a frame hitches (e.g. window drag).
        acc += fminf(GetFrameTime(), 0.25f);
        poll_input(world, cmds, camera);

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

        camera.target = champ_pos;
        render(world, champ_pos, alpha, camera);
    }

    CloseWindow();
    return 0;
}
