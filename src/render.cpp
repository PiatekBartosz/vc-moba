#include "render.hpp"

#include "raymath.h"
#include "rlgl.h"

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

// Gwen-style training dummy: wooden base + post, burlap sacks, rope ties, a painted
// bullseye on the chest, and a wooden cross-arm. Idle wobble (worse while burning).
static void draw_dummy_3d(const Dummy& d, const float t) {
    const Color wood{120, 80, 45, 255};
    const Color wood_dark{92, 60, 33, 255};
    const Color burlap{206, 184, 138, 255};
    const Color burlap_dark{168, 146, 102, 255};
    const Color rope{152, 122, 72, 255};
    const Color paint{182, 52, 46, 255};
    const Color paint_light{232, 232, 220, 255};

    const float phase = static_cast<float>(d.id) * 1.7f;
    const bool burning = d.ignite_left > 0.0f;
    const float sway = sinf(t * (burning ? 7.0f : 1.6f) + phase) * (burning ? 7.0f : 3.5f);
    const float bob = sinf(t * 2.2f + phase) * 1.5f;

    rlPushMatrix();
    rlTranslatef(d.pos.x, bob, d.pos.y);
    rlRotatef(sway, 0.0f, 0.0f, 1.0f);  // wobble toward/away from the camera

    // Base + central post.
    DrawCylinder(Vector3{0.0f, 0.0f, 0.0f}, 30.0f, 34.0f, 10.0f, 18, wood_dark);
    DrawCylinderWires(Vector3{0.0f, 0.0f, 0.0f}, 30.0f, 34.0f, 10.0f, 18, wood);
    DrawCylinder(Vector3{0.0f, 8.0f, 0.0f}, 5.0f, 5.0f, 64.0f, 10, wood);

    // Wooden cross-arm.
    DrawCube(Vector3{0.0f, 80.0f, 0.0f}, 124.0f, 8.0f, 8.0f, wood);
    DrawCubeWires(Vector3{0.0f, 80.0f, 0.0f}, 124.0f, 8.0f, 8.0f, wood_dark);

    // Burlap body: two stacked rounded sacks with a rope tie between them.
    DrawSphere(Vector3{0.0f, 56.0f, 0.0f}, 30.0f, burlap);
    DrawSphere(Vector3{0.0f, 92.0f, 0.0f}, 26.0f, burlap);
    DrawCylinder(Vector3{0.0f, 70.0f, 0.0f}, 27.5f, 27.5f, 6.0f, 18, rope);

    // Neck + head + top knot.
    DrawCylinder(Vector3{0.0f, 108.0f, 0.0f}, 10.0f, 10.0f, 8.0f, 12, rope);
    DrawSphere(Vector3{0.0f, 126.0f, 0.0f}, 19.0f, burlap);
    DrawSphere(Vector3{0.0f, 143.0f, 0.0f}, 7.0f, burlap_dark);

    // Painted bullseye on the chest (faces +Z, toward the camera).
    const float fz = 30.0f;
    DrawCircle3D(Vector3{0.0f, 92.0f, fz}, 16.0f, Vector3{1.0f, 0.0f, 0.0f}, 0.0f, paint);
    DrawCircle3D(Vector3{0.0f, 92.0f, fz}, 11.0f, Vector3{1.0f, 0.0f, 0.0f}, 0.0f, paint_light);
    DrawCircle3D(Vector3{0.0f, 92.0f, fz}, 6.0f, Vector3{1.0f, 0.0f, 0.0f}, 0.0f, paint);
    DrawSphere(Vector3{0.0f, 92.0f, fz}, 2.6f, paint);

    rlPopMatrix();
}

// Vayne-style night hunter, built from primitives: flared crimson coat, leather body,
// cape, wide-brim hat, and a crossbow held forward. Walk cycle + facing + windup glow.
static void draw_champion_3d(const Vector2 pos, const Champion& champ, const Vector2 facing,
                             const bool moving, const float t) {
    const Color coat{112, 30, 40, 255};
    const Color coat_dark{78, 20, 28, 255};
    const Color leather{30, 28, 36, 255};
    const Color skin{226, 190, 162, 255};
    const Color steel{156, 162, 178, 255};
    const Color glow{250, 230, 140, 255};

    Vector2 dir = facing;
    if (Vector2Length(dir) < 0.001f) {
        dir = Vector2{0.0f, 1.0f};
    }
    const float yaw = atan2f(dir.x, dir.y) * RAD2DEG;
    const float walk = moving ? sinf(t * 12.0f) : 0.0f;
    const float bob = moving ? fabsf(sinf(t * 12.0f)) * 2.5f : sinf(t * 2.0f) * 1.0f;

    rlPushMatrix();
    rlTranslatef(pos.x, bob, pos.y);
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);  // local +Z is "forward"

    // Legs (swing fore/aft when walking).
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(7.0f * side, 26.0f, 0.0f);
        rlRotatef(walk * 20.0f * side, 1.0f, 0.0f, 0.0f);
        DrawCylinder(Vector3{0.0f, -26.0f, 0.0f}, 5.0f, 6.0f, 26.0f, 10, leather);
        rlPopMatrix();
    }

    // Flared coat skirt + torso + belt.
    DrawCylinder(Vector3{0.0f, 16.0f, 0.0f}, 13.0f, 22.0f, 26.0f, 18, coat);
    DrawCylinderWires(Vector3{0.0f, 16.0f, 0.0f}, 13.0f, 22.0f, 26.0f, 18, coat_dark);
    DrawCylinder(Vector3{0.0f, 40.0f, 0.0f}, 12.0f, 13.0f, 20.0f, 14, leather);
    DrawSphere(Vector3{0.0f, 52.0f, 0.0f}, 13.0f, coat);
    DrawCylinder(Vector3{0.0f, 39.0f, 0.0f}, 13.5f, 13.5f, 4.0f, 16, leather);

    // Cape behind (local -Z), tilted out.
    rlPushMatrix();
    rlTranslatef(0.0f, 44.0f, -11.0f);
    rlRotatef(14.0f, 1.0f, 0.0f, 0.0f);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 26.0f, 44.0f, 3.0f, coat_dark);
    rlPopMatrix();

    // Shoulders + arms reaching forward to the crossbow.
    DrawSphere(Vector3{-13.0f, 60.0f, 0.0f}, 6.0f, leather);
    DrawSphere(Vector3{13.0f, 60.0f, 0.0f}, 6.0f, leather);
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(13.0f * side, 60.0f, 0.0f);
        rlRotatef(-65.0f - walk * 8.0f * side, 1.0f, 0.0f, 0.0f);
        DrawCylinder(Vector3{0.0f, -16.0f, 0.0f}, 4.0f, 4.0f, 16.0f, 8, leather);
        rlPopMatrix();
    }

    // Head + wide-brim hat.
    DrawSphere(Vector3{0.0f, 70.0f, 0.0f}, 8.0f, skin);
    DrawCylinder(Vector3{0.0f, 75.0f, 0.0f}, 16.0f, 16.0f, 2.5f, 20, leather);
    DrawCylinder(Vector3{0.0f, 77.0f, 0.0f}, 9.0f, 7.0f, 9.0f, 14, leather);

    // Crossbow held in front; bolt tip glows while winding up an attack.
    rlPushMatrix();
    rlTranslatef(0.0f, 50.0f, 16.0f);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 4.0f, 4.0f, 18.0f, steel);
    DrawCube(Vector3{0.0f, 0.0f, 6.0f}, 24.0f, 2.5f, 3.0f, steel);
    if (champ.windup > 0.0f) {
        DrawSphere(Vector3{0.0f, 0.0f, 12.0f}, 3.4f, glow);
    }
    rlPopMatrix();

    rlPopMatrix();
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

    const float t = static_cast<float>(GetTime());

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
        draw_dummy_3d(d, t);
        if (d.ignite_left > 0.0f) {  // burning indicator on the ground
            ground_ring(d.pos, d.radius + 6.0f, Color{240, 140, 40, 255});
            ground_ring(d.pos, d.radius + 11.0f, Color{220, 90, 30, 255});
        }
    }

    // Champion (Vayne-style). Face the velocity while moving, else the target.
    const Vector2 velocity = Vector2Subtract(champ_pos, w.champ.prev_pos);
    const bool moving = Vector2Length(velocity) > 0.05f;
    Vector2 facing{0.0f, 1.0f};
    if (moving) {
        facing = velocity;
    } else if (w.champ.order == Champion::Order::AttackTarget ||
               w.champ.order == Champion::Order::AttackMove) {
        for (const Dummy& d : w.dummies) {
            if (d.id == w.champ.target_id) {
                facing = Vector2Subtract(d.pos, champ_pos);
                break;
            }
        }
    }
    draw_champion_3d(champ_pos, w.champ, facing, moving, t);

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
