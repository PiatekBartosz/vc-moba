#include "render.hpp"

#include "raymath.h"
#include "rlgl.h"

#include "config.hpp"

// --- world-space (3D) helpers ------------------------------------------------

// A flat ring on the ground plane (range indicators, markers, buff/target rings).
static void ground_ring(const Vector2 center, const float radius, const Color c) {
    DrawCircle3D(to3d(center, 0.5f), radius, Vector3{1.0f, 0.0f, 0.0f}, 90.0f, c);
}

static float hash01(const int i) {
    const float x = sinf(static_cast<float>(i) * 12.9898f) * 43758.5453f;
    return x - floorf(x);
}

static Vector2 lerp2(const Vector2 a, const Vector2 b, const float s) {
    return Vector2{a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s};
}

// A flat, oriented band on the ground (used for the river).
static void draw_path(const Vector2 a, const Vector2 b, const float width, const float y,
                      const Color c) {
    const Vector2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    const Vector2 d{b.x - a.x, b.y - a.y};
    const float len = Vector2Length(d);
    const float yaw = atan2f(d.x, d.y) * RAD2DEG;
    rlPushMatrix();
    rlTranslatef(mid.x, y, mid.y);
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, width, 1.2f, len, c);
    rlPopMatrix();
}

// A raised paved road with darker curb edges and a faint center stripe.
static void draw_road(const Vector2 a, const Vector2 b, const float width) {
    const Vector2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    const Vector2 d{b.x - a.x, b.y - a.y};
    const float len = Vector2Length(d) + width;  // overlap at bends
    const float yaw = atan2f(d.x, d.y) * RAD2DEG;
    rlPushMatrix();
    rlTranslatef(mid.x, 0.0f, mid.y);
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);
    DrawCube(Vector3{0.0f, 1.5f, 0.0f}, width + 18.0f, 3.0f, len, Color{96, 84, 60, 255});   // curb
    DrawCube(Vector3{0.0f, 3.0f, 0.0f}, width, 3.0f, len, Color{168, 148, 104, 255});        // road
    DrawCube(Vector3{0.0f, 4.6f, 0.0f}, width * 0.10f, 1.0f, len, Color{206, 192, 150, 255}); // line
    rlPopMatrix();
}

// A jungle camp: a brushy circle with a couple of monster blobs (buff camps tinted).
static void draw_camp(const Vector2 p, const Color marker) {
    ground_ring(p, 58.0f, Color{60, 90, 60, 255});
    ground_ring(p, 50.0f, Color{40, 60, 40, 220});
    DrawSphere(to3d(p, 9.0f), 14.0f, marker);
    DrawSphere(to3d(Vector2{p.x - 22.0f, p.y + 11.0f}, 6.0f), 9.0f, Color{72, 98, 70, 255});
    DrawSphere(to3d(Vector2{p.x + 19.0f, p.y + 14.0f}, 6.0f), 8.0f, Color{72, 98, 70, 255});
}

// A pine tree: trunk + two stacked conical canopies (size jittered by seed).
static void draw_tree(const Vector2 p, const float s) {
    const Color trunk{96, 66, 40, 255};
    const Color leaf{42, 96, 56, 255};
    const Color leaf2{54, 116, 68, 255};
    DrawCylinder(to3d(p, 0.0f), 6.0f * s, 8.0f * s, 34.0f * s, 8, trunk);
    DrawCylinder(to3d(p, 26.0f * s), 0.0f, 26.0f * s, 36.0f * s, 10, leaf);
    DrawCylinder(to3d(p, 50.0f * s), 0.0f, 18.0f * s, 30.0f * s, 10, leaf2);
}

// A low brush cluster.
static void draw_bush(const Vector2 p, const float s) {
    const Color a{56, 104, 60, 255};
    const Color b{70, 122, 74, 255};
    DrawSphere(to3d(p, 9.0f * s), 18.0f * s, a);
    DrawSphere(to3d(Vector2{p.x - 14.0f * s, p.y + 8.0f * s}, 7.0f * s), 13.0f * s, b);
    DrawSphere(to3d(Vector2{p.x + 13.0f * s, p.y - 6.0f * s}, 7.0f * s), 12.0f * s, b);
}

// A turret: stone base + tapered body + crenellated crown with a glowing team eye.
static void draw_turret(const Vector2 p, const Color team) {
    const Color stone{120, 122, 132, 255};
    const Color stone_dark{84, 86, 96, 255};
    DrawCylinder(to3d(p, 0.0f), 42.0f, 50.0f, 22.0f, 12, stone_dark);
    DrawCylinder(to3d(p, 20.0f), 28.0f, 34.0f, 110.0f, 12, stone);
    DrawCylinderWires(to3d(p, 20.0f), 28.0f, 34.0f, 110.0f, 12, stone_dark);
    DrawCylinder(to3d(p, 128.0f), 38.0f, 30.0f, 18.0f, 12, stone_dark);  // crown
    for (int i = 0; i < 8; ++i) {
        const float a = static_cast<float>(i) * (PI / 4.0f);
        DrawCube(Vector3{p.x + cosf(a) * 32.0f, 150.0f, p.y + sinf(a) * 32.0f}, 10.0f, 14.0f,
                 10.0f, stone);
    }
    DrawSphere(to3d(p, 158.0f), 16.0f, team);  // glowing eye
}

// A team base: a platform ring, a nexus crystal, and two flanking nexus turrets.
static void draw_base(const Vector2 p, const Color team) {
    DrawCylinder(to3d(p, 0.0f), 200.0f, 220.0f, 9.0f, 6, Color{42, 46, 58, 255});
    ground_ring(p, 198.0f, team);
    DrawCylinder(to3d(p, 10.0f), 0.0f, 34.0f, 120.0f, 4, team);  // nexus crystal
}

static void draw_ground() {
    const float extent = k_arena_half_extent * 2.0f;
    DrawPlane(Vector3{0.0f, -0.5f, 0.0f}, Vector2{extent, extent}, Color{36, 64, 46, 255});  // grass

    const float h = 1850.0f;
    const Vector2 blue{-h, h};   // blue base corner
    const Vector2 red{h, -h};    // red base corner
    const Vector2 tl{-h, -h};
    const Vector2 br{h, h};
    const float lane_w = 190.0f;

    draw_path(tl, br, 320.0f, 0.05f, Color{52, 96, 150, 255});  // river

    draw_road(blue, red, lane_w);  // mid
    draw_road(blue, tl, lane_w);   // top (left edge)
    draw_road(tl, red, lane_w);    // top (top edge)
    draw_road(blue, br, lane_w);   // bot (bottom edge)
    draw_road(br, red, lane_w);    // bot (right edge)

    // Turrets: two per team along each lane.
    const Color t_blue{80, 140, 230, 255};
    const Color t_red{230, 110, 90, 255};
    draw_turret(lerp2(blue, red, 0.30f), t_blue);
    draw_turret(lerp2(blue, red, 0.45f), t_blue);
    draw_turret(lerp2(blue, red, 0.70f), t_red);
    draw_turret(lerp2(blue, red, 0.55f), t_red);
    draw_turret(lerp2(blue, tl, 0.55f), t_blue);
    draw_turret(lerp2(tl, red, 0.45f), t_red);
    draw_turret(lerp2(blue, br, 0.55f), t_blue);
    draw_turret(lerp2(br, red, 0.45f), t_red);

    // Jungle camps.
    const Color neutral{120, 150, 95, 255};
    const Color buff_blue{90, 150, 230, 255};
    const Color buff_red{230, 120, 80, 255};
    const Vector2 camps[8] = {
        {-820.0f, 320.0f}, {-320.0f, 820.0f},  {-1050.0f, 1020.0f}, {-420.0f, -980.0f},
        {820.0f, -320.0f}, {320.0f, -820.0f},  {1050.0f, -1020.0f}, {420.0f, 980.0f},
    };
    for (int i = 0; i < 8; ++i) {
        const Color m = (i == 0) ? buff_blue : (i == 4) ? buff_red : neutral;
        draw_camp(camps[i], m);
        // A small grove around each camp.
        for (int k = 0; k < 4; ++k) {
            const float a = static_cast<float>(k) * (PI / 2.0f) + hash01(i * 7 + k) * 1.5f;
            const float r = 120.0f + hash01(i * 13 + k) * 40.0f;
            draw_tree(Vector2{camps[i].x + cosf(a) * r, camps[i].y + sinf(a) * r},
                      0.9f + hash01(i * 5 + k) * 0.6f);
        }
        draw_bush(Vector2{camps[i].x + 70.0f, camps[i].y - 60.0f}, 1.0f);
    }

    // Forest framing the map edges.
    const float edge = h + 90.0f;
    for (int i = 0; i < 26; ++i) {
        const float u = -edge + (2.0f * edge) * (static_cast<float>(i) / 25.0f);
        const float j = (hash01(i * 3 + 1) - 0.5f) * 80.0f;
        const float sc = 1.1f + hash01(i * 9) * 0.8f;
        draw_tree(Vector2{u, edge + j}, sc);
        draw_tree(Vector2{u, -edge - j}, sc);
        draw_tree(Vector2{edge + j, u}, sc);
        draw_tree(Vector2{-edge - j, u}, sc);
    }

    draw_base(blue, Color{70, 130, 220, 255});
    draw_base(red, Color{220, 90, 70, 255});
}

// A jungle wall rendered as a craggy rock/mountain mass over its collision footprint:
// a solid base plus jittered peaks and boulders that break the silhouette, with moss.
static void draw_wall_rock(const Rectangle& wall, const int idx) {
    const float cx = wall.x + wall.width * 0.5f;
    const float cz = wall.y + wall.height * 0.5f;
    const float hgt = k_wall_height;
    const Color stone{106, 104, 114, 255};
    const Color stone_dark{74, 72, 82, 255};
    const Color moss{74, 100, 64, 255};

    // Solid mass matching the AABB used for collision.
    DrawCube(Vector3{cx, hgt * 0.5f, cz}, wall.width, hgt, wall.height, stone);
    DrawCubeWires(Vector3{cx, hgt * 0.5f, cz}, wall.width, hgt, wall.height, stone_dark);

    // Rocky peaks + boulders along the longer axis.
    const bool horiz = wall.width >= wall.height;
    const float span = horiz ? wall.width : wall.height;
    const float girth = horiz ? wall.height : wall.width;
    const int peaks = 2 + static_cast<int>(span / 170.0f);
    for (int i = 0; i < peaks; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(peaks);
        const float jit = (hash01(idx * 31 + i) - 0.5f) * girth * 0.35f;
        const Vector2 p = horiz ? Vector2{wall.x + wall.width * u, cz + jit}
                                : Vector2{cx + jit, wall.y + wall.height * u};
        const float ph = hgt * (1.0f + hash01(idx * 17 + i) * 0.7f);  // peak top height
        const float pr = girth * (0.36f + hash01(idx * 7 + i) * 0.18f);
        DrawCylinder(Vector3{p.x, hgt * 0.55f, p.y}, 0.0f, pr, ph - hgt * 0.55f, 6, stone);
        DrawSphere(Vector3{p.x, hgt * 0.95f, p.y}, pr * 0.65f, stone_dark);
        DrawSphere(Vector3{p.x + pr * 0.5f, hgt * 0.55f, p.y - pr * 0.3f}, pr * 0.45f, stone);
        if (hash01(idx * 5 + i) > 0.5f) {  // moss patch on some peaks
            DrawSphere(Vector3{p.x, ph - pr * 0.4f, p.y}, pr * 0.4f, moss);
        }
    }
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

    // Stun stars orbiting the head (Condemn wall slam).
    if (d.stun_left > 0.0f) {
        for (int i = 0; i < 3; ++i) {
            const float ang = t * 6.0f + static_cast<float>(i) * 2.094f;
            DrawSphere(Vector3{cosf(ang) * 16.0f, 158.0f, sinf(ang) * 16.0f}, 4.0f,
                       Color{250, 220, 90, 255});
        }
    }

    rlPopMatrix();
}

// Vayne-style night hunter, built entirely from primitives: boots, a tailored flared
// coat with tails, bandolier, popped collar, pauldrons, bent arms with gauntlets, a
// wide-brim hat, a two-point cape, and a curved crossbow. Walk cycle + facing + glow.
static void draw_champion_3d(const Vector2 pos, const Champion& champ, const Vector2 facing,
                             const bool moving, const float t, const bool stealth, const float roll) {
    Color coat{124, 30, 42, 255};
    Color coat_dark{84, 18, 28, 255};
    Color leather{28, 26, 34, 255};
    Color leather2{46, 42, 54, 255};
    Color skin{228, 194, 168, 255};
    Color steel{154, 162, 182, 255};
    Color steel_dark{92, 98, 116, 255};
    Color gold{214, 172, 76, 255};
    Color glow{250, 230, 140, 255};
    Color scarf{158, 34, 44, 255};

    if (stealth) {  // Final Hour invisibility: render faded.
        const float a = 0.32f;
        coat = Fade(coat, a);
        coat_dark = Fade(coat_dark, a);
        leather = Fade(leather, a);
        leather2 = Fade(leather2, a);
        skin = Fade(skin, a);
        steel = Fade(steel, a);
        steel_dark = Fade(steel_dark, a);
        gold = Fade(gold, a);
        glow = Fade(glow, a);
        scarf = Fade(scarf, a);
    }

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
    rlScalef(k_champ_model_scale, k_champ_model_scale, k_champ_model_scale);
    if (roll != 0.0f) {  // Tumble: forward somersault about the body's mid-height
        const float pivot = 42.0f;
        rlTranslatef(0.0f, pivot, 0.0f);
        rlRotatef(roll, 1.0f, 0.0f, 0.0f);
        rlTranslatef(0.0f, -pivot, 0.0f);
    }

    // Legs + boots (swing fore/aft when walking).
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(7.0f * side, 30.0f, 0.0f);
        rlRotatef(walk * 22.0f * side, 1.0f, 0.0f, 0.0f);
        DrawCylinder(Vector3{0.0f, -30.0f, 0.0f}, 5.0f, 6.5f, 30.0f, 10, leather);
        DrawCylinder(Vector3{0.0f, -8.0f, 0.0f}, 7.5f, 7.5f, 6.0f, 10, leather2);  // boot cuff
        DrawCube(Vector3{0.0f, -29.0f, 4.0f}, 11.0f, 6.0f, 16.0f, leather);        // foot
        rlPopMatrix();
    }

    // Flared coat skirt + front/back tails.
    DrawCylinder(Vector3{0.0f, 18.0f, 0.0f}, 13.0f, 23.0f, 28.0f, 20, coat);
    DrawCylinderWires(Vector3{0.0f, 18.0f, 0.0f}, 13.0f, 23.0f, 28.0f, 20, coat_dark);
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        DrawCube(Vector3{10.0f * side, 14.0f, 12.0f}, 12.0f, 30.0f, 3.0f, coat);   // front tails
    }
    DrawCube(Vector3{0.0f, 10.0f, -13.0f}, 22.0f, 34.0f, 3.0f, coat_dark);          // back tail

    // Torso + waist + belt + buckle.
    DrawCylinder(Vector3{0.0f, 42.0f, 0.0f}, 12.0f, 13.5f, 22.0f, 16, leather);
    DrawSphere(Vector3{0.0f, 54.0f, 0.0f}, 13.0f, coat);
    DrawCylinder(Vector3{0.0f, 41.0f, 0.0f}, 14.0f, 14.0f, 5.0f, 18, leather2);
    DrawCube(Vector3{0.0f, 43.0f, 13.0f}, 6.0f, 6.0f, 3.0f, gold);                  // buckle

    // Bandolier strap across the chest.
    rlPushMatrix();
    rlTranslatef(0.0f, 52.0f, 11.5f);
    rlRotatef(32.0f, 0.0f, 0.0f, 1.0f);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 5.0f, 34.0f, 3.0f, leather2);
    rlPopMatrix();

    // Popped collar + red scarf at the neck.
    DrawCylinder(Vector3{0.0f, 62.0f, 0.0f}, 9.0f, 7.0f, 8.0f, 12, scarf);
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(6.0f * side, 60.0f, -4.0f);
        rlRotatef(18.0f * side, 0.0f, 0.0f, 1.0f);
        DrawCube(Vector3{0.0f, 8.0f, 0.0f}, 6.0f, 18.0f, 4.0f, coat);
        rlPopMatrix();
    }

    // Two-point cape behind.
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(7.0f * side, 46.0f, -12.0f);
        rlRotatef(16.0f, 1.0f, 0.0f, 0.0f);
        rlRotatef(-4.0f * side, 0.0f, 0.0f, 1.0f);
        DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 15.0f, 46.0f, 2.5f, coat_dark);
        rlPopMatrix();
    }

    // Pauldrons.
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        DrawSphere(Vector3{14.0f * side, 62.0f, 0.0f}, 7.5f, leather2);
        DrawSphere(Vector3{15.0f * side, 64.0f, 0.0f}, 4.0f, coat);
    }

    // Arms: upper arm angled in, forearm reaching to the crossbow, gauntlet cuff.
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(14.0f * side, 60.0f, 0.0f);
        rlRotatef(-58.0f - walk * 8.0f * side, 1.0f, 0.0f, 0.0f);
        DrawCylinder(Vector3{0.0f, -14.0f, 0.0f}, 4.5f, 4.0f, 15.0f, 8, leather);   // upper arm
        rlTranslatef(0.0f, -14.0f, 0.0f);
        rlRotatef(-32.0f, 1.0f, 0.0f, 0.0f);
        DrawCylinder(Vector3{0.0f, -13.0f, 0.0f}, 4.0f, 3.5f, 13.0f, 8, leather);   // forearm
        DrawCylinder(Vector3{0.0f, -13.0f, 0.0f}, 4.6f, 4.6f, 4.0f, 10, leather2);  // gauntlet
        rlPopMatrix();
    }

    // Neck + head + face shadow.
    DrawCylinder(Vector3{0.0f, 66.0f, 0.0f}, 4.5f, 4.5f, 6.0f, 8, skin);
    DrawSphere(Vector3{0.0f, 74.0f, 0.0f}, 8.5f, skin);
    DrawSphere(Vector3{0.0f, 70.0f, -7.0f}, 6.0f, coat_dark);  // hair behind

    // Wide-brim hat: brim disc, band, tapered cap.
    DrawCylinder(Vector3{0.0f, 79.0f, 0.0f}, 17.0f, 17.0f, 2.5f, 22, leather);
    DrawCylinder(Vector3{0.0f, 81.0f, 0.0f}, 10.0f, 10.0f, 3.0f, 16, scarf);        // hat band
    DrawCylinder(Vector3{0.0f, 84.0f, 0.0f}, 10.0f, 7.0f, 10.0f, 16, leather);

    // Crossbow held in front: stock, two curved limbs, string, loaded bolt.
    rlPushMatrix();
    rlTranslatef(0.0f, 52.0f, 18.0f);
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, 4.0f, 4.0f, 22.0f, steel_dark);             // stock
    DrawCube(Vector3{0.0f, -3.0f, -6.0f}, 3.0f, 7.0f, 4.0f, steel_dark);           // grip
    for (int s = -1; s <= 1; s += 2) {
        const float side = static_cast<float>(s);
        rlPushMatrix();
        rlTranslatef(0.0f, 0.0f, 9.0f);
        rlRotatef(58.0f * side, 0.0f, 0.0f, 1.0f);
        DrawCylinder(Vector3{0.0f, 0.0f, 0.0f}, 1.6f, 1.6f, 16.0f, 6, steel);       // bow limb
        rlPopMatrix();
    }
    // String from each limb tip back to the stock nock.
    const Vector3 nock{0.0f, 0.0f, -2.0f};
    DrawLine3D(Vector3{-13.5f, 8.0f, 9.0f}, nock, Color{210, 210, 220, 255});
    DrawLine3D(Vector3{13.5f, 8.0f, 9.0f}, nock, Color{210, 210, 220, 255});
    // Loaded bolt; tip glows while winding up.
    DrawCylinder(Vector3{0.0f, 0.0f, -2.0f}, 1.0f, 1.0f, 18.0f, 6, gold);
    if (champ.windup > 0.0f) {
        DrawSphere(Vector3{0.0f, 0.0f, 16.0f}, 3.6f, glow);
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
    const float total_w = size * 4.0f + gap * 3.0f;
    const float x0 = (static_cast<float>(k_window_size_x) - total_w) * 0.5f;
    const float y = static_cast<float>(k_window_size_y) - size - 28.0f;
    const bool practice = !w.cooldowns_enabled;
    const Cooldown passive{};  // Silver Bolts is passive -> always shown ready

    draw_ability_slot(x0, y, size, "Q", "Tumble", c.tumble, practice);
    draw_ability_slot(x0 + (size + gap), y, size, "W", "Silver", passive, true);
    draw_ability_slot(x0 + (size + gap) * 2.0f, y, size, "E", "Condemn", c.condemn, practice);
    draw_ability_slot(x0 + (size + gap) * 3.0f, y, size, "R", "Final", c.ult, practice);

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

    for (int i = 0; i < static_cast<int>(w.walls.size()); ++i) {
        draw_wall_rock(w.walls[static_cast<size_t>(i)], i);
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
    if (w.champ.ult_left > 0.0f) {  // Final Hour active
        ground_ring(champ_pos, k_champ_radius + 13.0f, Color{200, 110, 235, 255});
    }

    for (const Dummy& d : w.dummies) {
        draw_dummy_3d(d, t);
        if (d.ignite_left > 0.0f) {  // burning indicator on the ground
            ground_ring(d.pos, d.radius + 6.0f, Color{240, 140, 40, 255});
            ground_ring(d.pos, d.radius + 11.0f, Color{220, 90, 30, 255});
        }
        // Silver Bolts stacks: one silver ring per stack on the single stacked target.
        if (d.id == w.champ.sb_target_id && w.champ.sb_stacks > 0) {
            for (int s = 0; s < w.champ.sb_stacks; ++s) {
                ground_ring(d.pos, d.radius + 16.0f + static_cast<float>(s) * 9.0f,
                            Color{210, 218, 240, 255});
            }
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
    float roll = 0.0f;
    if (w.champ.dash_left > 0.0f) {  // Tumble: one full forward roll over the dash
        const float prog = 1.0f - w.champ.dash_left / k_q_dash_duration;
        roll = prog * 360.0f;
    }
    draw_champion_3d(champ_pos, w.champ, facing, moving, t, w.champ.stealth_left > 0.0f, roll);

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
        "RMB: move/attack  A+LMB: attack-move  Q: tumble  E: condemn  R: ultimate  D: flash  F: ghost  T: dummy  N: no-cd  X: reset",
        10, 35, 20, Color{200, 200, 210, 255});
    EndDrawing();
}
