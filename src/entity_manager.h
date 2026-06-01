#pragma once
#include "ncurses_wrapper.h"
#include <vector>
#include <random>
#include <chrono>

// ─── Entity types ─────────────────────────────────────────────────────────────
enum EntityType : int {
    ENT_CAR_MAG = 0,   // magenta NPC car (2×2)
    ENT_CAR_CYN,       // cyan NPC car    (2×2)
    ENT_CAR_ORG,       // orange NPC car  (2×2)
    ENT_TRUCK,         // yellow truck    (2×4)
    ENT_CONE,          // traffic cone    (1×1) ▲
    ENT_BARRICADE,     // barricade       (4×1) alternating red/white
    ENT_TRAFFIC_LIGHT, // traffic light (vertical 3x1)
    ENT_STOP_SIGN,     // STOP sign (1x1 red)
    ENT_YIELD_SIGN,    // YIELD sign (1x1 yellow triangle)
    ENT_POLICE,        // Police car (2x3) with flashing red/blue lights
    ENT_CAR_HORIZ      // Horizontal traffic car (4×2, moves left/right through intersection)
};

// ─── Decoration element (trees, water patches, buildings, etc.) ───────────────
struct Decor {
    enum Kind { TREE, WATER, HOUSE, BUILDING, PARKING, SIGN_POST } kind;
    float y;      // screen y (float for smooth scroll)
    float fx;     // screen x (float for smooth scroll in horiz mode)
    int   w, h;   // size in cells
    int   color_variant = 0; // for houses/buildings: 0,1,2 = different colours
};

// ─── NPC / Obstacle entity ────────────────────────────────────────────────────
struct Entity {
    EntityType type;
    float y;         // top-left y (float for smooth movement)
    float fx;        // top-left x as float (used by horiz cars)
    int   x;         // top-left x (int – snapped to lane)
    int   w, h;      // bounding box in cells
    float speed;     // cells/second (positive = moves down toward player)
    float hspeed;    // horizontal speed cells/sec (ENT_CAR_HORIZ only)
    int   lane;      // 0..3
    bool  active;

    // NPC traffic-rule state
    bool  npc_stopping  = false;  // NPC is slowing/stopped for red light or stop sign
    float npc_orig_speed= 0.f;   // original speed before stopping
};

// ─── Player ───────────────────────────────────────────────────────────────────
struct Player {
    float x;         // top-left x (float for smooth lateral slide)
    float y;         // top-left y
    int   w = 2, h = 3;
    int   lane = 2;  // current target lane (0..3), 0,1=oncoming, 2,3=forward
    float target_x;  // smooth interpolation target
    bool  alive = true;
};

// ─── Road-side Traffic Light (existing, scrolls with road) ───────────────────
struct TrafficLight {
    float y;
    int   x;
    float cycle_timer = 0.f;
    int   state = 0; // 0=GREEN, 1=YELLOW, 2=RED
    bool  active = false;
    bool  is_green()  const { return state == 0; }
    bool  is_yellow() const { return state == 1; }
    bool  is_red()    const { return state == 2; }
};

// ─── Road-center Semaphore (mounted above intersection, full traffic light) ───
struct Semaphore {
    float y;           // row where the semaphore is (scrolls with road)
    int   x;           // col (placed at center divider of road)
    float cycle_timer  = 0.f;
    int   state        = 0;  // 0=GREEN, 1=YELLOW, 2=RED
    bool  active       = false;
    bool  is_green()   const { return state == 0; }
    bool  is_yellow()  const { return state == 1; }
    bool  is_red()     const { return state == 2; }
};

// ─── EntityManager ────────────────────────────────────────────────────────────
class EntityManager {
public:
    Player            player;
    std::vector<Entity> npcs;
    std::vector<Entity> obstacles;
    std::vector<Entity> horiz_cars;   // horizontal intersection traffic
    std::vector<Decor>  decors;
    TrafficLight        tlight;
    Semaphore           semaphore;    // new: center-road semaphore

    void init(const ScreenDims& d);
    void reset(const ScreenDims& d);

    void set_horiz(bool h) { is_horiz_ = h; }

    // dt in seconds, player_speed_kmh = player's speed in km/h
    void update(float dt, float player_speed_kmh, int move_dir, const ScreenDims& dims, int heading);

    // Draw all entities onto the game window
    // horiz_dir: 0 = vertical, 1 = east, -1 = west
    void draw(WINDOW* w, int scroll_off, const ScreenDims& d, int horiz_dir = 0) const;

    bool player_collides(const ScreenDims& d) const;
    
    void on_intersection_spawned(float y, bool traffic_light_is_green, int int_h = 6);
    bool is_traffic_light_green() const { return !tlight.active || tlight.is_green(); }

    // Returns true if any police NPC is currently on-screen (visible)
    bool any_police_on_screen() const {
        for (const auto& e : npcs)
            if (e.active && e.type == ENT_POLICE &&
                e.y >= -4.f && e.y < (float)(dims_.rows + 4))
                return true;
        return false;
    }

    // Returns true if player overlaps an active horizontal car
    bool player_collides_horiz() const;

    // Returns true if the semaphore is active and red
    bool semaphore_is_red() const { return semaphore.active && semaphore.is_red(); }

private:
    ScreenDims    dims_;
    std::mt19937  rng_;
    float         npc_timer_    = 0.f;
    float         obs_timer_    = 0.f;
    float         decor_timer_  = 0.f;
    float         env_timer_    = 0.f;  // environment spawn timer

    bool is_horiz_ = false;

    void spawn_npc();
    void spawn_obstacle();
    void spawn_decor(float start_y = -999.f);
    void spawn_env_element();           // houses, buildings, parking, signs
    void spawn_horiz_cars(float y, bool light_is_green);

    // NPC compliance: slow / stop NPCs for red light or stop signs
    void update_npc_compliance(float dt, float road_scroll);

    void draw_player   (WINDOW* w, int horiz_dir) const;
    void draw_npc      (WINDOW* w, const Entity& e) const;
    void draw_obstacle (WINDOW* w, const Entity& e) const;
    void draw_decor    (WINDOW* w, const Decor&  dec) const;
    void draw_horiz_car(WINDOW* w, const Entity& e) const;
    void draw_semaphore(WINDOW* w) const;

    bool rects_overlap(float ay, int ax, int aw, int ah,
                       float by, int bx, int bw, int bh) const;
};
