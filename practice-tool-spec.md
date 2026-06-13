# LoL Practice Tool — Build Spec (C++ / raylib)

> Handoff doc for an implementing agent (Claude Code). Build it **phase by phase**;
> every phase must compile and run before moving on. Keep all game logic on the
> fixed `DT` step — never read `GetFrameTime()` outside the loop accumulator.

## 1. Goal

A small top-down League-of-Legends "practice tool" clone: one player champion,
point-and-click movement, ranged auto-attacks you can **kite** with, the summoner
spells **Flash / Ghost / Ignite**, and **placeable target dummies** to attack. No
game engine — raylib is just a library; we write our own loop and systems.

## 2. Tech / build

- Language: C++20. Library: **raylib** only.
- Arch Linux: `sudo pacman -S raylib cmake`
- Quick build while single-file or small: `g++ -std=c++20 src/*.cpp -o practice -lraylib`
- CMake (`find_package(raylib)`) once files are split (Phase 1+).
- Top-down 2D view via `Camera2D` centered on the champion. World units == pixels.
  Mouse → world via `GetScreenToWorld2D(GetMousePosition(), camera)`.

## 3. Architecture principles

- **Data lives in a `World` struct; behavior lives in stateless `system` functions**
  that take `(World&, float dt)`. No deep inheritance, no `virtual update()`.
- **Fixed timestep.** Render rate varies; simulation must not, or kiting timing
  becomes frame-rate-dependent. Accumulate wall-clock time, step the sim in fixed
  `DT` slices, render once per frame.
- The frame order is explicit and central (see §4). That ordering is the design.

```cpp
const float DT = 1.0f / 60.0f;
float acc = 0.0f;
while (!WindowShouldClose()) {
    acc += GetFrameTime();
    Commands cmds = pollInput(world, camera);   // raw input -> intents
    while (acc >= DT) {
        update(world, cmds, DT);                // runs the ordered systems
        cmds.consumeOneShots();                 // flash/ignite/place fire once
        acc -= DT;
    }
    render(world, camera);
}
```

## 4. Per-frame system order (inside `update`)

1. Apply commands → set champion order (MoveToPoint / AttackTarget), queue one-shots.
2. Abilities + cooldowns tick (flash/ghost/ignite, decrement cooldowns & buffs).
3. Movement (toward order target, scaled by move speed + ghost buff).
4. Auto-attack state machine (chase → windup → fire).
5. Projectiles advance → apply damage on hit.
6. Effects (ignite DoT, buff expiry) + cleanup (dead projectiles, popups).

## 5. Data model

```cpp
struct Cooldown { float remaining = 0, total = 0;
    bool ready() const { return remaining <= 0; }
    void trigger() { remaining = total; }
    void tick(float dt) { if (remaining > 0) remaining -= dt; } };

struct Champion {
    Vector2 pos{};
    float   moveSpeed     = 340.f;   // units/sec
    float   attackRange   = 550.f;   // ranged -> kiteable
    float   attacksPerSec = 0.8f;    // cycle time = 1/AS
    float   atkDamage     = 60.f;
    float   hp = 600, maxHp = 600;

    enum class Order { Idle, MoveToPoint, AttackTarget };
    Order   order = Order::Idle;
    Vector2 movePoint{};
    int     targetId = -1;

    float   atkCooldown = 0.f;       // time until next attack allowed
    float   windup      = 0.f;       // >0 == mid-attack; damage fires when it hits 0
    Cooldown flash, ghost, ignite;
    float   ghostBuff   = 0.f;       // seconds of move-speed buff remaining
};

struct Dummy { int id; Vector2 pos; float radius = 28.f, hp, maxHp;
               float igniteLeft = 0.f, igniteDps = 0.f; };

struct Projectile { Vector2 pos; int targetId; float speed = 1800.f, dmg; bool live = true; };

struct FloatingText { Vector2 pos; float value, life = 0.8f; };

struct World {
    Champion champ;
    std::vector<Dummy> dummies;
    std::vector<Projectile> projectiles;
    std::vector<Rectangle> walls;
    std::vector<FloatingText> popups;
    bool cooldownsEnabled = true;    // practice toggle (false == no cooldowns)
    int  nextId = 0;
};
```

## 6. Combat state machine + kiting (the core — get this exact)

LoL timing model: an attack has a **windup**; damage applies at the END of the
windup; the remaining cycle is free "recovery" you can cancel by moving. Moving
**before** the damage point cancels the attack (wasted); moving **after** is a clean
kite. Ranged autos are **homing projectiles**, so you can move while they travel —
that is *why* kiting works.

```cpp
void combatSystem(World& w, float dt) {
    Champion& c = w.champ;
    c.atkCooldown -= dt;

    if (c.windup > 0.f) {                       // committed; locked in place
        c.windup -= dt;
        if (c.windup <= 0.f)
            spawnProjectile(w, c.targetId, c.atkDamage);   // damage point
        return;
    }
    if (c.order != Champion::Order::AttackTarget) return;
    Dummy* t = findDummy(w, c.targetId);
    if (!t) { c.order = Champion::Order::Idle; return; }

    float d = Vector2Distance(c.pos, t->pos);
    if (d > c.attackRange) {
        stepToward(c, t->pos, dt);              // chase into range
    } else if (c.atkCooldown <= 0.f) {
        c.windup      = 0.3f * (1.f / c.attacksPerSec);     // begin attack
        c.atkCooldown = 1.f / c.attacksPerSec;
    }                                           // in range + on CD -> stand still
}
```

A new MoveToPoint command sets `order = MoveToPoint`. If it arrives while `windup > 0`
(before the damage point) it should zero `windup` → attack cancelled. After the
damage point `windup` is already 0, so moving is free → kite.

```cpp
void projectileSystem(World& w, float dt) {
    for (auto& p : w.projectiles) {
        Dummy* t = findDummy(w, p.targetId);
        if (!t) { p.live = false; continue; }
        p.pos = Vector2MoveTowards(p.pos, t->pos, p.speed * dt);
        if (Vector2Distance(p.pos, t->pos) < 6.f) { applyDamage(w, *t, p.dmg); p.live = false; }
    }
    std::erase_if(w.projectiles, [](const Projectile& p){ return !p.live; });
}
```

## 7. Feature spec

**Controls (configurable in config.hpp):**
- Right-click empty ground → move there. Right-click on a dummy → attack it (chase + kite).
- `D` = Flash, `F` = Ghost, `R` = Ignite (point-and-click: press R, then left-click a dummy).
- `T` = place a target dummy at the cursor.
- `A` = attack-move (Phase 3): click a point, walk there but auto-engage dummies in range.
- `N` = toggle no-cooldown practice mode (`cooldownsEnabled`).
- `X` = reset (heal champion, reset/refill dummies).

**Flash:** instant blink toward cursor, capped at `FLASH_RANGE` (or to the cursor if
closer). Ignores wall collision (can flash over walls); if the landing point lands
inside a wall, nudge it out to the nearest free spot. Sets `flash` cooldown.

**Ghost:** sets `ghostBuff = GHOST_DURATION`; `movementSystem` multiplies move speed by
`(1 + GHOST_BONUS)` while `ghostBuff > 0`. Sets `ghost` cooldown.

**Ignite:** targeted DoT. On apply, set the dummy's `igniteLeft = IGNITE_DURATION`,
`igniteDps = IGNITE_DPS`. Effects system does `hp -= igniteDps * dt` each step while
`igniteLeft > 0`. Sets `ignite` cooldown.

**Cooldowns:** all three share the `Cooldown` helper and one HUD row with a radial/linear
sweep. When `cooldownsEnabled == false`, abilities ignore `remaining` (always castable).

**Dummies:** static circles with an HP bar; take auto-attack + ignite damage; show
floating damage numbers. Optionally slow HP regen, or just clamp at 0 / reset on `X`.

**Walls (Phase 3):** axis-aligned `Rectangle`s. Champion movement collides (stop/slide);
Flash ignores them. Include at least one wall to practice flashing over.

**HUD:** champion HP bar, ability bar (Flash/Ghost/Ignite with cooldown sweep + key
labels), attack-range indicator circle around the champion, no-cooldown-mode indicator.

## 8. Tunable constants (`config.hpp`) — starting values

```cpp
constexpr float MOVE_SPEED      = 340.f;
constexpr float ATTACK_RANGE    = 550.f;
constexpr float ATTACKS_PER_SEC = 0.8f;
constexpr float ATTACK_DAMAGE   = 60.f;
constexpr float PROJECTILE_SPEED= 1800.f;
constexpr float CHAMP_HP        = 600.f;
constexpr float DUMMY_HP        = 1000.f;

constexpr float FLASH_RANGE     = 400.f;
constexpr float FLASH_CD        = 300.f;
constexpr float GHOST_BONUS     = 0.30f;   // +30% move speed
constexpr float GHOST_DURATION  = 10.f;
constexpr float GHOST_CD        = 210.f;
constexpr float IGNITE_DPS      = 50.f;
constexpr float IGNITE_DURATION = 5.f;
constexpr float IGNITE_CD       = 180.f;
```

## 9. File layout

```
practice-tool/
  CMakeLists.txt
  src/
    main.cpp      loop + fixed timestep
    world.hpp     structs + Cooldown + World
    config.hpp    all tunables
    input.cpp     raylib input -> Commands
    systems.cpp   movement / combat / projectiles / abilities / effects
    render.cpp    camera, world draw, HUD
```
Start single-file; split at these seams when it gets uncomfortable. Don't over-modularize early.

## 10. Phased roadmap (each phase compiles and runs)

**Phase 0 — foundation.** Window, fixed-timestep loop, `Camera2D` following champion,
draw arena grid + champion circle, right-click to move (MoveToPoint + `stepToward`).
*Done when:* you can click around and the champion walks there smoothly at a fixed step.

**Phase 1 — combat & kiting.** Place dummies (`T`), the auto-attack state machine,
homing projectiles, HP bars, floating damage numbers, attack-range indicator.
*Done when:* you can attack a dummy and orb-walk/kite it without cancelling autos early.

**Phase 2 — summoner spells.** Flash, Ghost, Ignite + `Cooldown` helper + HUD ability bar
with cooldown sweeps. *Done when:* all three cast, respect cooldowns, and show on the HUD.

**Phase 3 — terrain & polish.** Walls + collision, flash-over-wall, attack-move (`A`),
no-cooldown toggle (`N`), reset (`X`). *Done when:* it feels like a real practice tool.

## 11. Gotchas

- All durations/speeds use `DT`, never `GetFrameTime()`, inside systems.
- One-shot commands (flash, ignite, place dummy) must fire once per frame, not once
  per fixed substep — consume them after the substep loop.
- Keep render read-only w.r.t. the simulation; it draws state, never mutates it.
- `std::erase_if` needs `<vector>`/`<algorithm>` (C++20).
