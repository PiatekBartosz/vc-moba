#include "render.hpp"

#include "raymath.h"

#include "config.hpp"

// --- world-space (3D) helpers ------------------------------------------------

// A flat ring on the ground plane (range indicators, markers, buff/target rings).
static void ground_ring(const Vector2 center, const float radius, const Color c) {
    DrawCircle3D(to3d(center, 0.5f), radius, Vector3{1.0f, 0.0f, 0.0f}, 90.0f, c);
}

static void draw_ground() {
    const float extent = k_arena_half_extent * 2.0f;
    DrawPlane(Vector3{0.0f, -0.5f, 0.0f}, Vector2{extent, extent}, Color{26, 28, 36, 255});
    DrawGrid(static_cast<int>(extent / k_grid_spacing), k_grid_spacing);
}

static void draw_dummy_3d(const Dummy& d) {
    const Vector3 base = to3d(d.pos, 0.0f);
    DrawCylinder(base, d.radius, d.radius, k_dummy_height, 16, Color{170, 110, 90, 255});
    DrawCylinderWires(base, d.radius, d.radius, k_dummy_height, 16, Color{230, 180, 160, 255});
    if (d.ignite_left > 0.0f) {  // burning indicator
        ground_ring(d.pos, d.radius + 5.0f, Color{240, 140, 40, 255});
        ground_ring(d.pos, d.radius + 9.0f, Color{220, 90, 30, 255});
    }
}

// --- screen-space (2D) helpers -----------------------------------------------

// HP bar drawn in screen space above an entity (its 3D head projected to 2D).
static void draw_health_bar_screen(const Vector2 sp, const float hp, const float max_hp) {
    const float bw = 56.0f;
    const float bh = 7.0f;
    const float x = sp.x - bw * 0.5f;
    const float y = sp.y;
    const float frac = max_hp > 0.0f ? Clamp(hp / max_hp, 0.0f, 1.0f) : 0.0f;

    DrawRectangleRec(Rectangle{x - 1.0f, y - 1.0f, bw + 2.0f, bh + 2.0f}, Color{0, 0, 0, 180});
    DrawRectangleRec(Rectangle{x, y, bw, bh}, Color{60, 20, 20, 255});
    DrawRectangleRec(Rectangle{x, y, bw * frac, bh}, Color{210, 60, 60, 255});
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

static void draw_reticle(const Vector2 m, const Color c) {
    DrawCircleLinesV(m, 17.0f, c);
    DrawCircleLinesV(m, 10.0f, c);
    DrawLineV(Vector2{m.x - 22.0f, m.y}, Vector2{m.x - 13.0f, m.y}, c);
    DrawLineV(Vector2{m.x + 22.0f, m.y}, Vector2{m.x + 13.0f, m.y}, c);
    DrawLineV(Vector2{m.x, m.y - 22.0f}, Vector2{m.x, m.y - 13.0f}, c);
    DrawLineV(Vector2{m.x, m.y + 22.0f}, Vector2{m.x, m.y + 13.0f}, c);
    DrawCircleV(m, 2.0f, c);
}

static void draw_cursor(const bool attack_armed, const bool ignite_armed) {
    const Vector2 m = GetMousePosition();

    if (attack_armed) {  // League-style attack-move reticle
        draw_reticle(m, Color{235, 80, 70, 255});
        return;
    }
    if (ignite_armed) {  // ignite targeting reticle
        draw_reticle(m, Color{245, 150, 50, 255});
        return;
    }

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

// --- frame --------------------------------------------------------------------

void render(const World& w, const Vector2 champ_pos, const float alpha,
            const bool attack_move_armed, const bool ignite_armed, const Camera3D& camera) {
    BeginDrawing();
    ClearBackground(Color{14, 15, 20, 255});

    BeginMode3D(camera);
    draw_ground();

    for (const Rectangle& wall : w.walls) {
        const Vector3 center{wall.x + wall.width * 0.5f, k_wall_height * 0.5f,
                             wall.y + wall.height * 0.5f};
        DrawCube(center, wall.width, k_wall_height, wall.height, Color{55, 58, 70, 255});
        DrawCubeWires(center, wall.width, k_wall_height, wall.height, Color{95, 100, 120, 255});
    }

    // Ground rings: attack range, order markers, target highlight, ghost buff.
    ground_ring(champ_pos, w.champ.attack_range, Color{80, 110, 150, 160});
    if (w.champ.order == Champion::Order::MoveToPoint) {
        ground_ring(w.champ.move_point, 12.0f, Color{120, 200, 120, 255});
    }
    if (w.champ.order == Champion::Order::AttackMove) {
        ground_ring(w.champ.move_point, 12.0f, Color{235, 100, 90, 255});
    }
    if (w.champ.order == Champion::Order::AttackTarget ||
        w.champ.order == Champion::Order::AttackMove) {
        for (const Dummy& d : w.dummies) {
            if (d.id == w.champ.target_id) {
                ground_ring(d.pos, d.radius + 6.0f, Color{240, 210, 80, 255});
                break;
            }
        }
    }
    if (w.champ.ghost_buff > 0.0f) {
        ground_ring(champ_pos, k_champ_radius + 8.0f, Color{120, 240, 150, 255});
    }

    for (const Dummy& d : w.dummies) {
        draw_dummy_3d(d);
    }

    // Champion: a cylinder that turns gold while winding up an attack.
    const Vector3 champ_base = to3d(champ_pos, 0.0f);
    const Color champ_fill =
        w.champ.windup > 0.0f ? Color{230, 200, 90, 255} : Color{90, 150, 240, 255};
    DrawCylinder(champ_base, k_champ_radius, k_champ_radius, k_champ_height, 20, champ_fill);
    DrawCylinderWires(champ_base, k_champ_radius, k_champ_radius, k_champ_height, 20,
                      Color{200, 220, 255, 255});

    // Projectiles (interpolated), drawn as airborne spheres.
    for (const Projectile& p : w.projectiles) {
        const Vector2 pp = Vector2Lerp(p.prev_pos, p.pos, alpha);
        DrawSphere(to3d(pp, k_proj_height), k_projectile_radius, Color{250, 230, 140, 255});
    }
    EndMode3D();

    // Screen-space billboards: HP bars above dummies, floating damage numbers.
    for (const Dummy& d : w.dummies) {
        const Vector2 sp = GetWorldToScreen(to3d(d.pos, k_dummy_height + 34.0f), camera);
        draw_health_bar_screen(sp, d.hp, d.max_hp);
    }
    for (const FloatingText& f : w.popups) {
        const Vector2 sp = GetWorldToScreen(to3d(f.pos, k_dummy_height * 0.6f), camera);
        const float rise = (k_popup_life - f.life) * k_popup_rise;
        const unsigned char a =
            static_cast<unsigned char>(Clamp(f.life / k_popup_life, 0.0f, 1.0f) * 255.0f);
        DrawText(TextFormat("%.0f", static_cast<double>(f.value)), static_cast<int>(sp.x) - 14,
                 static_cast<int>(sp.y - rise), 24, Color{255, 240, 160, a});
    }

    draw_ability_bar(w);

    draw_cursor(attack_move_armed, ignite_armed);
    DrawFPS(10, 10);
    DrawText(
        "RMB: move/attack  A+LMB: attack-move  R+LMB: ignite  T: dummy  D: flash  F: ghost  N: no-cd  X: reset",
        10, 35, 20, Color{200, 200, 210, 255});
    EndDrawing();
}
