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

inline constexpr float k_dummy_radius = 28.0f;
inline constexpr float k_dummy_hp = 1000.0f;

struct Champion {
    Vector2 pos{0.0f, 0.0f};
    Vector2 prev_pos{0.0f, 0.0f};  // sim position one step ago, for render interpolation
    float move_speed = k_move_speed;

    enum class Order { Idle, MoveToPoint };
    Order order = Order::Idle;
    Vector2 move_point{0.0f, 0.0f};
};

struct Dummy {
    int id = 0;
    Vector2 pos{0.0f, 0.0f};
    float radius = k_dummy_radius;
    float hp = k_dummy_hp;
    float max_hp = k_dummy_hp;
};

struct World {
    Champion champ;
    std::vector<Dummy> dummies;
    int next_id = 0;
};

struct Commands {
    bool move_requested = false;
    Vector2 move_point{0.0f, 0.0f};

    bool place_requested = false;
    Vector2 place_point{0.0f, 0.0f};

    // Fired once per frame (consumed after each substep), so a placement issued on
    // a frame with no fixed step is held until a step runs, never duplicated.
    void consume_one_shots() { place_requested = false; }
};

static void poll_input(Commands& cmds, const Camera2D& camera) {
    // Hold-to-move (LoL style): while the right button is held, keep re-issuing the
    // move order toward the cursor. A single tap still works; holding makes it robust.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        cmds.move_requested = true;
        cmds.move_point = GetScreenToWorld2D(GetMousePosition(), camera);
    }
    if (IsKeyPressed(KEY_T)) {
        cmds.place_requested = true;
        cmds.place_point = GetScreenToWorld2D(GetMousePosition(), camera);
    }
}

static void spawn_dummy(World& w, const Vector2 pos) {
    w.dummies.push_back(Dummy{w.next_id++, pos});
}

static void step_toward(Champion& c, const Vector2 target, const float dt) {
    c.pos = Vector2MoveTowards(c.pos, target, c.move_speed * dt);
}

static void update(World& w, const Commands& cmds, const float dt) {
    Champion& champ = w.champ;
    champ.prev_pos = champ.pos;

    if (cmds.move_requested) {
        champ.order = Champion::Order::MoveToPoint;
        champ.move_point = cmds.move_point;
    }
    if (cmds.place_requested) {
        spawn_dummy(w, cmds.place_point);
    }

    if (champ.order == Champion::Order::MoveToPoint) {
        step_toward(champ, champ.move_point, dt);
        if (Vector2Distance(champ.pos, champ.move_point) <= k_arrive_epsilon) {
            champ.order = Champion::Order::Idle;
        }
    }
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

static void render(const World& w, const Vector2 champ_pos, const Camera2D& camera) {
    BeginDrawing();
    ClearBackground(Color{18, 18, 22, 255});

    BeginMode2D(camera);
    draw_grid();
    for (const Dummy& d : w.dummies) {
        draw_dummy(d);
    }
    if (w.champ.order == Champion::Order::MoveToPoint) {
        DrawCircleV(w.champ.move_point, 5.0f, Color{120, 200, 120, 255});
    }
    DrawCircleV(champ_pos, k_champ_radius, Color{90, 150, 240, 255});
    DrawCircleLinesV(champ_pos, k_champ_radius, Color{200, 220, 255, 255});
    EndMode2D();

    draw_cursor();
    DrawFPS(10, 10);
    DrawText("Right-click: move    T: place dummy", 10, 35, 20, Color{200, 200, 210, 255});
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
        poll_input(cmds, camera);

        bool stepped = false;
        while (acc >= k_dt) {
            update(world, cmds, k_dt);
            cmds.consume_one_shots();  // one-shots fire once, on the first substep
            acc -= k_dt;
            stepped = true;
        }
        if (stepped) {
            cmds.move_requested = false;  // movement re-latched each frame while held
        }

        // Render interpolation: draw between the last two sim states by how far we
        // are into the next fixed step, so motion is smooth at any render rate.
        const float alpha = acc / k_dt;
        const Vector2 champ_pos = Vector2Lerp(world.champ.prev_pos, world.champ.pos, alpha);

        camera.target = champ_pos;
        render(world, champ_pos, camera);
    }

    CloseWindow();
    return 0;
}
