#define _XOPEN_SOURCE_EXTENDED 1
#include "entity_manager.h"
#include <algorithm>
#include <cmath>

// ─── Helpers ──────────────────────────────────────────────────────────────────
bool EntityManager::rects_overlap(float ay, int ax, int aw, int ah,
                                   float by, int bx, int bw, int bh) const {
    float ax2 = ax + aw, ay2 = ay + ah;
    float bx2 = bx + bw, by2 = by + bh;
    return ax < bx2 && ax2 > bx && ay < by2 && ay2 > by;
}

// ─── Init / Reset ─────────────────────────────────────────────────────────────
void EntityManager::init(const ScreenDims& d) {
    dims_ = d;
    unsigned seed = (unsigned)std::chrono::steady_clock::now().time_since_epoch().count();
    rng_.seed(seed);
    reset(d);
}

void EntityManager::reset(const ScreenDims& d) {
    dims_ = d;
    npcs.clear();
    obstacles.clear();
    horiz_cars.clear();
    decors.clear();
    npc_timer_   = 0.f;
    obs_timer_   = 0.f;
    decor_timer_ = 0.f;
    env_timer_   = 0.f;

    player.x        = (float)d.player_start_x;
    player.y        = (float)d.player_start_y;
    player.target_x = player.x;
    player.lane     = 2;
    player.alive    = true;

    tlight.active    = false;
    semaphore.active = false;

    // Pre-seed decor and environment elements
    for (int i = 0; i < 10; i++) spawn_decor(i * (d.rows / 10.f));
    for (int i = 0; i < 5;  i++) spawn_env_element();
}

// ─── Spawning ─────────────────────────────────────────────────────────────────
void EntityManager::spawn_npc() {
    Entity e;
    int type_rand = rand() % 100;
    if (type_rand < 55)      e.type = ENT_CAR_CYN;
    else if (type_rand < 85) e.type = ENT_CAR_ORG;
    else if (type_rand < 95) e.type = ENT_POLICE;
    else                     e.type = ENT_TRUCK;

    e.w = 2; e.h = 3;
    if (e.type == ENT_TRUCK) { e.w = 2; e.h = 4; }
    if (is_horiz_) {
        if (e.type == ENT_TRUCK) { e.w = 6; e.h = 2; }
        else { e.w = 4; e.h = 2; }
    }

    int lane = rand() % 4;
    e.lane = lane;
    e.active = true;
    e.npc_stopping = false;

    if (!is_horiz_) {
        e.y = -5.f;
        e.x = (int)(dims_.lane_cx[lane] - e.w / 2);
        e.fx = (float)e.x;
        if (lane < 2) e.speed = -(40.f + rand() % 40);  // fast oncoming
        else          e.speed = 60.f + rand() % 50;       // same dir
        e.hspeed = 0;
    } else {
        e.y = (float)(dims_.horiz_cy[lane] - e.h / 2);
        if (lane < 2) {
            e.fx    = (float)(dims_.game_cols + 5);
            e.speed = -(40.f + rand() % 40);
        } else {
            e.fx    = (float)(-e.w - 5);
            e.speed = 60.f + rand() % 50;
        }
        e.x = (int)e.fx;
        e.hspeed = 0;
    }
    e.npc_orig_speed = e.speed;

    // Check for overlap at spawn
    bool overlap = false;
    for (const auto& other : npcs) {
        if (other.active && rects_overlap(e.y, (int)e.fx, e.w, e.h,
                                          other.y, (int)other.fx, other.w, other.h)) {
            overlap = true; break;
        }
    }
    if (!overlap) npcs.push_back(e);
}

void EntityManager::spawn_obstacle() {
    std::uniform_int_distribution<int> type_dist(0, 2);
    std::uniform_int_distribution<int> count_dist(1, 2);

    int num_obs = count_dist(rng_);
    std::vector<int> lanes = {0, 1, 2, 3};
    std::shuffle(lanes.begin(), lanes.end(), rng_);

    for (int i = 0; i < num_obs; i++) {
        int t = type_dist(rng_);
        EntityType etype = (t <= 1) ? ENT_CONE : ENT_BARRICADE;
        int li = lanes[i];

        Entity e;
        e.type   = etype;
        e.lane   = li;
        e.speed  = 0.f;
        e.hspeed = 0.f;
        e.active = true;
        e.npc_stopping = false;

        e.w = (etype == ENT_CONE) ? 1 : 2;
        e.h = 1;

        if (!is_horiz_) {
            e.y  = -2.f - (i * 4.f);
            e.x  = dims_.lane_cx[li];
            if (etype == ENT_BARRICADE) e.x -= 1;
            e.fx = (float)e.x;
        } else {
            e.fx = (float)(dims_.game_cols + 5 + i * 4);
            e.y = (float)(dims_.horiz_cy[li] - e.h / 2);
            e.x = (int)e.fx;
        }

        obstacles.push_back(e);
    }
}

void EntityManager::spawn_decor(float start_pos) {
    std::uniform_int_distribution<int> kind_dist(0, 3);
    std::uniform_int_distribution<int> side_dist(0, 1);

    Decor d;
    d.kind = (kind_dist(rng_) == 3) ? Decor::WATER : Decor::TREE;
    d.color_variant = rand() % 3;

    if (!is_horiz_) {
        d.y = (start_pos == -999.f) ? -5.f : start_pos;
        int side = side_dist(rng_);
        if (d.kind == Decor::TREE) {
            d.w = 2; d.h = 3;
            d.fx = (float)((side == 0)
                ? rand() % std::max(1, dims_.curb_lx - 3)
                : dims_.grass_rx + rand() % std::max(1, dims_.game_cols - dims_.grass_rx - 3));
        } else {
            d.w = 3; d.h = 2;
            d.fx = (float)((side == 0)
                ? rand() % std::max(1, dims_.curb_lx - 3)
                : dims_.grass_rx + rand() % std::max(1, dims_.game_cols - dims_.grass_rx - 3));
        }
    } else {
        d.fx = (start_pos == -999.f) ? (float)dims_.game_cols + 5.f : start_pos;
        int road_total = 4 * dims_.lane_w + 3;
        int side_y = std::max(0, (dims_.rows - road_total - 2) / 2);
        int curb_uy = side_y;
        int curb_dy = side_y + road_total + 1;
        int side = side_dist(rng_);

        if (d.kind == Decor::TREE) {
            d.w = 2; d.h = 3;
        } else {
            d.w = 3; d.h = 2;
        }

        if (side == 0) {
            int max_y = curb_uy - d.h;
            d.y = (max_y > 0) ? (float)(rand() % max_y) : (float)(max_y - 1);
        } else {
            d.y = (curb_dy + 1) + rand() % std::max(1, dims_.rows - (curb_dy + 1) - d.h);
        }
    }
    decors.push_back(d);
}

// ─── Environment elements (houses, buildings, parking, signs) ─────────────────
void EntityManager::spawn_env_element() {
    std::uniform_int_distribution<int> kind_dist(0, 5);
    std::uniform_int_distribution<int> side_dist(0, 1);

    int k = kind_dist(rng_);
    Decor d;
    d.color_variant = rand() % 3;
    int side = side_dist(rng_);

    if (k <= 1) {
        d.kind = Decor::HOUSE;
        d.w = 4; d.h = 4;
    } else if (k <= 3) {
        d.kind = Decor::BUILDING;
        d.w = 5; d.h = 6;
    } else {
        d.kind = Decor::PARKING;
        d.w = 6; d.h = 4;
    }

    if (!is_horiz_) {
        d.y = -d.h - 2.f;
        if (side == 0) {
            int max_x = dims_.curb_lx - d.w - 1;
            if (max_x < 0) max_x = 0;
            d.fx = (float)(rand() % std::max(1, max_x));
        } else {
            int min_x = dims_.grass_rx + 1;
            int max_x = dims_.game_cols - d.w - 1;
            if (max_x < min_x) max_x = min_x;
            d.fx = (float)(min_x + rand() % std::max(1, max_x - min_x));
        }
    } else {
        d.fx = (float)dims_.game_cols + 5.f;
        int curb_uy = dims_.horiz_curb_uy;
        int curb_dy = dims_.horiz_curb_dy;
        if (side == 0) {
            int max_y = curb_uy - d.h;
            d.y = (max_y > 0) ? (float)(rand() % max_y) : (float)(max_y - 1);
        } else {
            int min_y = curb_dy + 1;
            int max_y = dims_.rows - d.h - 1;
            if (max_y < min_y) max_y = min_y;
            d.y = (float)(min_y + rand() % std::max(1, max_y - min_y));
        }
    }
    decors.push_back(d);
}

// ─── Horizontal car spawning ──────────────────────────────────────────────────
void EntityManager::spawn_horiz_cars(float y, bool light_is_green) {
    if (light_is_green) return;

    std::uniform_int_distribution<int> count_dist(1, 3);
    std::uniform_int_distribution<int> color_dist(0, 2);
    std::uniform_real_distribution<float> spd_dist(8.f, 14.f);

    int n = count_dist(rng_);
    for (int i = 0; i < n; i++) {
        Entity e;
        EntityType types[3] = { ENT_CAR_MAG, ENT_CAR_CYN, ENT_CAR_ORG };
        e.type   = types[color_dist(rng_)];
        e.lane   = 0;
        e.active = true;
        e.npc_stopping = false;

        if (!is_horiz_) {
            e.w = 4; e.h = 2;
            e.speed  = 0.f;
            e.hspeed = spd_dist(rng_);
            float start_offset = (float)(-dims_.game_cols/2 - i * 8);
            e.fx = start_offset;
            e.x  = (int)e.fx;
            e.y  = y;
        } else {
            e.w = 2; e.h = 3;
            e.speed = spd_dist(rng_) * 4.f;
            e.hspeed = 0.f;
            float start_offset = (float)(-dims_.rows/2 - i * 6);
            e.y = start_offset;
            e.fx = y + dims_.lane_h;
            e.x = (int)e.fx;
        }
        e.npc_orig_speed = e.speed;
        horiz_cars.push_back(e);
    }
}

// ─── NPC compliance: NPCs in forward lanes slow/stop at red lights & stop signs ─
void EntityManager::update_npc_compliance(float dt, float road_scroll) {
    (void)dt;
    (void)road_scroll;
    // Determine relevant stop thresholds
    float sem_y    = semaphore.active ? semaphore.y : -9999.f;
    float tlight_y = tlight.active   ? tlight.y    : -9999.f;
    bool  red      = (semaphore.active && semaphore.is_red()) ||
                     (tlight.active && tlight.is_red());

    // Find stop sign y
    float stop_sign_y = -9999.f;
    for (const auto& obs : obstacles) {
        if (obs.active && obs.type == ENT_STOP_SIGN) {
            stop_sign_y = obs.y;
            break;
        }
    }

    for (auto& e : npcs) {
        if (!e.active) continue;
        // Only forward-direction lanes (2,3) need to comply
        if (e.lane < 2) continue;
        // Only vertical mode for now
        if (is_horiz_) continue;

        float front_y = e.y; // NPC front (top of sprite when coming from top)
        // Distance to stop line
        float dist_to_sem    = sem_y    - front_y;
        float dist_to_tlight = tlight_y - front_y;
        float dist_to_stop   = stop_sign_y - front_y;

        // Choose the closest active stop trigger ahead of NPC
        float stop_dist = 9999.f;
        if (red && dist_to_sem > 0.f && dist_to_sem < stop_dist)
            stop_dist = dist_to_sem;
        if (red && dist_to_tlight > 0.f && dist_to_tlight < stop_dist)
            stop_dist = dist_to_tlight;
        if (dist_to_stop > 0.f && dist_to_stop < stop_dist)
            stop_dist = dist_to_stop;

        if (stop_dist < 12.f) {
            // Slow down proportionally to distance
            float brake = 1.f - (stop_dist / 12.f);
            e.speed = e.npc_orig_speed * (1.f - brake);
            if (e.speed < 0.f) e.speed = 0.f;
            e.npc_stopping = true;
        } else if (e.npc_stopping) {
            // Resume when no stop reason
            e.speed = e.npc_orig_speed;
            e.npc_stopping = false;
        }
    }
}


void EntityManager::update(float dt, float player_speed_kmh, int move_dir,
                            const ScreenDims& d, int heading) {
    dims_ = d;
    float road_scroll_cells = player_speed_kmh * 0.15f;
    bool going_west = (is_horiz_ && heading == 3);
    float h_dir = going_west ? -1.f : 1.f;

    // ── Player lateral movement ──────────────────────────────────────────────
    if (move_dir != 0) {
        int new_lane = player.lane + move_dir;
        if (new_lane >= 0 && new_lane <= 3) {
            player.lane = new_lane;
            player.target_x = (float)(d.lane_cx[player.lane] - 1);
        }
    }
    float dx = player.target_x - player.x;
    float slide = std::min(std::abs(dx), 20.f * dt);
    player.x += (dx > 0 ? slide : -slide);

    // ── NPC compliance logic ──────────────────────────────────────────────────
    update_npc_compliance(dt, road_scroll_cells);

    // ── NPC movement ─────────────────────────────────────────────────────────
    for (auto& e : npcs) {
        if (!e.active) continue;
        if (!is_horiz_) {
            float relative_kmh = player_speed_kmh - e.speed;
            e.y += (relative_kmh * 0.15f) * dt;
            if (e.y > d.rows + 2 || e.y < -15.f) e.active = false;
        } else {
            float relative_kmh = player_speed_kmh - e.speed;
            e.fx -= (relative_kmh * 0.15f) * dt * h_dir;
            e.x = (int)e.fx;
            if (e.fx < -(float)(e.w + 20) || e.fx > (float)(d.game_cols + 20)) e.active = false;
        }
    }

    // ── Traffic light update ─────────────────────────────────────────────────
    if (tlight.active) {
        if (!is_horiz_) {
            tlight.y += road_scroll_cells * dt;
        }
        tlight.cycle_timer += dt;
        if (tlight.state == 0 && tlight.cycle_timer > 5.f) { tlight.state = 1; tlight.cycle_timer = 0.f; }
        else if (tlight.state == 1 && tlight.cycle_timer > 2.f) { tlight.state = 2; tlight.cycle_timer = 0.f; }
        else if (tlight.state == 2 && tlight.cycle_timer > 5.f) { tlight.state = 0; tlight.cycle_timer = 0.f; }

        if (tlight.y > d.rows + 5) tlight.active = false;
    }

    // ── Semaphore update (scrolls with road, full cycle) ─────────────────────
    if (semaphore.active) {
        semaphore.y += road_scroll_cells * dt;
        semaphore.cycle_timer += dt;
        if (semaphore.state == 0 && semaphore.cycle_timer > 6.f)  { semaphore.state = 1; semaphore.cycle_timer = 0.f; }
        else if (semaphore.state == 1 && semaphore.cycle_timer > 2.f) { semaphore.state = 2; semaphore.cycle_timer = 0.f; }
        else if (semaphore.state == 2 && semaphore.cycle_timer > 5.f) { semaphore.state = 0; semaphore.cycle_timer = 0.f; }

        if (semaphore.y > d.rows + 5) semaphore.active = false;
    }

    // ── Obstacle scroll ─────────────────────────────────────────────────────
    for (auto& e : obstacles) {
        if (!e.active) continue;
        if (!is_horiz_) {
            e.y += road_scroll_cells * dt;
            if (e.y > d.rows + 2) e.active = false;
        } else {
            e.fx -= road_scroll_cells * dt * h_dir;
            e.x = (int)e.fx;
            if (e.fx < -10.f || e.fx > d.game_cols + 20.f) e.active = false;
        }
    }

    // ── Horizontal cars (intersection traffic) ────────────────────────────────
    for (auto& e : horiz_cars) {
        if (!e.active) continue;
        if (!is_horiz_) {
            e.y  += road_scroll_cells * dt;
            e.fx += e.hspeed * dt;
            e.x   = (int)e.fx;
            if (e.y > d.rows + 4 || e.fx > d.game_cols + 8 || e.fx < -e.w - 4)
                e.active = false;
        } else {
            e.fx -= road_scroll_cells * dt * h_dir;
            e.x   = (int)e.fx;
            e.y  += e.speed * dt;
            if (e.fx < -10.f || e.fx > d.game_cols + 20.f || e.y > d.rows + 4 || e.y < -d.rows)
                e.active = false;
        }
    }

    // ── Decor scroll ─────────────────────────────────────────────────────────
    for (auto& dec : decors) {
        if (!is_horiz_) {
            dec.y += road_scroll_cells * dt;
        } else {
            dec.fx -= road_scroll_cells * dt * h_dir;
        }
    }

    // Remove off-screen
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
        [](const Entity& e){ return !e.active; }), npcs.end());
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
        [](const Entity& e){ return !e.active; }), obstacles.end());
    horiz_cars.erase(std::remove_if(horiz_cars.begin(), horiz_cars.end(),
        [](const Entity& e){ return !e.active; }), horiz_cars.end());

    if (!is_horiz_) {
        decors.erase(std::remove_if(decors.begin(), decors.end(),
            [&d](const Decor& dec){ return dec.y > d.rows + 6; }), decors.end());
    } else {
        if (going_west) {
            decors.erase(std::remove_if(decors.begin(), decors.end(),
                [&d](const Decor& dec){ return dec.fx > d.game_cols + 10.f; }), decors.end());
        } else {
            decors.erase(std::remove_if(decors.begin(), decors.end(),
                [](const Decor& dec){ return dec.fx < -10.f; }), decors.end());
        }
    }

    // ── Spawn timers ─────────────────────────────────────────────────────────
    npc_timer_   += dt;
    obs_timer_   += dt;
    decor_timer_ += dt;
    env_timer_   += dt;

    if (npc_timer_ > ((rand() % 10) / 10.f + 0.5f)) {
        npc_timer_ = 0.f;
        spawn_npc();
        if (npcs.size() > 14) npcs.erase(npcs.begin());
    }
    if (obs_timer_ > 4.0f) {
        obs_timer_ = 0.f;
        spawn_obstacle();
        if (obstacles.size() > 8) obstacles.erase(obstacles.begin());
    }
    if (decor_timer_ > 1.0f) {
        decor_timer_ = 0.f;
        spawn_decor();
        if (decors.size() > 40) decors.erase(decors.begin());
    }
    if (env_timer_ > 2.5f) {
        env_timer_ = 0.f;
        spawn_env_element();
        if (decors.size() > 40) decors.erase(decors.begin());
    }
}

// ─── Drawing ─────────────────────────────────────────────────────────────────
void EntityManager::draw_player(WINDOW* w, int horiz_dir) const {
    int px = (int)player.x;
    int py = (int)player.y;

    auto safe_fill = [&](int r, int c, int cp) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            fill_block(w, r, c, cp);
    };

    if (horiz_dir == 0) {
        safe_fill(py,   px,   CP_PLAYER_HL);
        safe_fill(py,   px+1, CP_PLAYER_HL);
        safe_fill(py+1, px,   CP_PLAYER);
        safe_fill(py+1, px+1, CP_PLAYER);
        safe_fill(py+2, px,   CP_PLAYER);
        safe_fill(py+2, px+1, CP_PLAYER);
    } else {
        for (int dy = 0; dy < 2; dy++) {
            for (int ddx = 0; ddx < 4; ddx++) {
                int cp = CP_PLAYER;
                if (horiz_dir == 1  && ddx == 3 && dy == 0) cp = CP_PLAYER_HL;
                if (horiz_dir == -1 && ddx == 0 && dy == 0) cp = CP_PLAYER_HL;
                safe_fill(py + dy, px + ddx, cp);
            }
        }
    }
}

void EntityManager::draw_npc(WINDOW* w, const Entity& e) const {
    int py = (int)e.y;
    int px = e.x;

    auto safe_fill = [&](int r, int c, int cp) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            fill_block(w, r, c, cp);
    };

    int body_cp = (e.type == ENT_CAR_MAG) ? CP_NPC_MAG :
                  (e.type == ENT_CAR_CYN) ? CP_NPC_CYN :
                  (e.type == ENT_CAR_ORG) ? CP_NPC_ORG : CP_TRUCK;

    if (e.type == ENT_TRUCK) {
        if (!is_horiz_) {
            for (int dy = 0; dy < 4; dy++) {
                int cp = (dy == 1) ? CP_TRUCK_W : CP_TRUCK;
                safe_fill(py + dy, px,   cp);
                safe_fill(py + dy, px+1, cp);
            }
        } else {
            for (int dy = 0; dy < 2; dy++) {
                for (int ddx = 0; ddx < e.w; ddx++) {
                    int cp = (ddx == 1 || ddx == 2) ? CP_TRUCK_W : CP_TRUCK;
                    safe_fill(py + dy, px + ddx, cp);
                }
            }
        }
    } else if (e.type == ENT_POLICE) {
        // Police car: WHITE body, alternating red/blue roof lights
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        bool flash = (ms % 400) < 200; // fast flash
        if (!is_horiz_) {
            // Roof: left cell RED, right cell BLUE, alternating each flash
            safe_fill(py,   px,   flash ? CP_TLIGHT_RED : CP_TLIGHT_OFF);
            safe_fill(py,   px+1, flash ? CP_TLIGHT_OFF : CP_POLICE_FLSH);
            // White body
            safe_fill(py+1, px,   CP_POLICE_WHT);
            safe_fill(py+1, px+1, CP_POLICE_WHT);
            safe_fill(py+2, px,   CP_POLICE_BODY); // dark window strip
            safe_fill(py+2, px+1, CP_POLICE_BODY);
        } else {
            // Horiz: top row = alternating red/blue lights
            for (int ddx = 0; ddx < 4; ddx++) {
                bool red_slot = (ddx < 2);
                safe_fill(py,   px+ddx, (red_slot == flash) ? CP_TLIGHT_RED : CP_POLICE_FLSH);
            }
            // White body
            for (int ddx = 0; ddx < 4; ddx++)
                safe_fill(py+1, px+ddx, CP_POLICE_WHT);
        }
    } else {
        if (!is_horiz_) {
            for (int dy = 0; dy < 3; dy++) {
                safe_fill(py + dy, px,   body_cp);
                safe_fill(py + dy, px+1, body_cp);
            }
        } else {
            for (int dy = 0; dy < 2; dy++) {
                for (int ddx = 0; ddx < 4; ddx++) {
                    safe_fill(py + dy, px + ddx, body_cp);
                }
            }
        }
    }
}

void EntityManager::draw_obstacle(WINDOW* w, const Entity& e) const {
    int py = (int)e.y;
    if (py < 0 || py >= dims_.rows) return;

    if (e.type == ENT_CONE) {
        draw_wch(w, py, e.x, CP_CONE, L'\u25B2');
    } else if (e.type == ENT_BARRICADE) {
        if (!is_horiz_) {
            for (int ddx = 0; ddx < e.w; ddx++) {
                int cp = (ddx % 2 == 0) ? CP_BARIC_R : CP_BARIC_W;
                fill_block(w, py, e.x + ddx, cp);
            }
        } else {
            for (int dy = 0; dy < e.h; dy++) {
                if (py + dy < 0 || py + dy >= dims_.rows) continue;
                int cp = (dy % 2 == 0) ? CP_BARIC_R : CP_BARIC_W;
                fill_block(w, py + dy, e.x, cp);
            }
        }
    } else if (e.type == ENT_STOP_SIGN) {
        // STOP sign: red octagon-style, 2 wide × 2 tall + pole
        // Row 0: corners cut off with border chars to suggest octagon
        if (py >= 0 && py < dims_.rows) {
            fill_block(w, py, e.x,   CP_STOP_SIGN);
            fill_block(w, py, e.x+1, CP_STOP_SIGN);
            draw_wch  (w, py, e.x,   CP_STOP_SIGN, L'S');
            draw_wch  (w, py, e.x+1, CP_STOP_SIGN, L'T');
        }
        if (py+1 >= 0 && py+1 < dims_.rows) {
            fill_block(w, py+1, e.x,   CP_STOP_SIGN);
            fill_block(w, py+1, e.x+1, CP_STOP_SIGN);
            draw_wch  (w, py+1, e.x,   CP_STOP_SIGN, L'O');
            draw_wch  (w, py+1, e.x+1, CP_STOP_SIGN, L'P');
        }
        // Pole
        if (py+2 >= 0 && py+2 < dims_.rows)
            draw_wch(w, py+2, e.x, CP_TREE_TRNK, L'|');
    } else if (e.type == ENT_YIELD_SIGN) {
        // YIELD sign: yellow inverted triangle symbol + "YD" text + pole
        // Row 0: big downward triangle character (the universal yield symbol)
        if (py >= 0 && py < dims_.rows) {
            fill_block(w, py, e.x,   CP_YIELD_SIGN);
            fill_block(w, py, e.x+1, CP_YIELD_SIGN);
            draw_wch  (w, py, e.x,   CP_YIELD_SIGN, L'\u25BD'); // ▽ outlined
            draw_wch  (w, py, e.x+1, CP_YIELD_SIGN, L'\u25BD');
        }
        // Row 1: "YD" label on yellow background
        if (py+1 >= 0 && py+1 < dims_.rows) {
            fill_block(w, py+1, e.x,   CP_YIELD_SIGN);
            fill_block(w, py+1, e.x+1, CP_YIELD_SIGN);
            draw_wch  (w, py+1, e.x,   CP_YIELD_SIGN, L'Y');
            draw_wch  (w, py+1, e.x+1, CP_YIELD_SIGN, L'D');
        }
        // Pole
        if (py+2 >= 0 && py+2 < dims_.rows)
            draw_wch(w, py+2, e.x, CP_TREE_TRNK, L'|');
    }
}

void EntityManager::draw_semaphore(WINDOW* w) const {
    if (!semaphore.active) return;
    int sy = (int)semaphore.y;
    int sx = semaphore.x;

    auto safe_fill = [&](int r, int c, int cp) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            fill_block(w, r, c, cp);
    };

    // Horizontal overhead semaphore: backing board
    for (int i = -3; i <= 3; i++) {
        safe_fill(sy, sx + i, CP_INTERSECTION);
    }

    int cp_r = (semaphore.state == 2) ? CP_TLIGHT_RED : CP_TLIGHT_OFF;
    int cp_y = (semaphore.state == 1) ? CP_TLIGHT_YEL : CP_TLIGHT_OFF;
    int cp_g = (semaphore.state == 0) ? CP_TLIGHT_GRN : CP_TLIGHT_OFF;

    safe_fill(sy, sx - 2, cp_r);
    safe_fill(sy, sx,     cp_y);
    safe_fill(sy, sx + 2, cp_g);
}

void EntityManager::draw_decor(WINDOW* w, const Decor& dec) const {
    int py = (int)dec.y;
    int px = (int)dec.fx;

    auto safe_fill = [&](int r, int c, int cp) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            fill_block(w, r, c, cp);
    };
    auto safe_wch = [&](int r, int c, int cp, wchar_t ch) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            draw_wch(w, r, c, cp, ch);
    };

    switch (dec.kind) {
    case Decor::TREE:
        for (int dy = 0; dy < 2; dy++)
            for (int ddx = 0; ddx < dec.w; ddx++)
                safe_fill(py + dy, px + ddx, CP_TREE_TOP);
        safe_fill(py + 2, px + 1, CP_TREE_TRNK);
        break;

    case Decor::WATER:
        for (int dy = 0; dy < dec.h; dy++)
            for (int ddx = 0; ddx < dec.w; ddx++)
                safe_fill(py + dy, px + ddx, CP_WATER);
        break;

    case Decor::HOUSE: {
        for (int ddx = 0; ddx < dec.w; ddx++)
            safe_fill(py, px + ddx, CP_HOUSE_ROOF);
        for (int dy = 1; dy < dec.h; dy++)
            for (int ddx = 0; ddx < dec.w; ddx++)
                safe_fill(py + dy, px + ddx, CP_HOUSE_WALL);
        safe_wch(py + dec.h - 1, px + dec.w/2, CP_TREE_TRNK, L'\u2502');
        if (dec.h >= 3) {
            safe_wch(py + 1, px + 1,          CP_BUILDING_W, L'\u25A1');
            safe_wch(py + 1, px + dec.w - 2,  CP_BUILDING_W, L'\u25A1');
        }
        break;
    }

    case Decor::BUILDING: {
        for (int dy = 0; dy < dec.h; dy++)
            for (int ddx = 0; ddx < dec.w; ddx++)
                safe_fill(py + dy, px + ddx, CP_BUILDING);
        for (int dy = 0; dy < dec.h - 1; dy += 2)
            for (int ddx = 1; ddx < dec.w - 1; ddx += 2)
                safe_wch(py + dy, px + ddx, CP_BUILDING_W, L'\u25A0');
        break;
    }

    case Decor::PARKING: {
        for (int dy = 0; dy < dec.h; dy++)
            for (int ddx = 0; ddx < dec.w; ddx++)
                safe_fill(py + dy, px + ddx, CP_PARKING);
        for (int ddx = 0; ddx < dec.w; ddx += 2)
            for (int dy = 0; dy < dec.h; dy++)
                safe_fill(py + dy, px + ddx, CP_PARKING_LN);
        safe_wch(py, px + dec.w/2, CP_SIGN_POST, L'P');
        break;
    }
    case Decor::SIGN_POST:
        break;
    }
}

void EntityManager::draw(WINDOW* w, int /*scroll_off*/, const ScreenDims& /*d*/, int horiz_dir) const {
    // Decor first (behind everything)
    for (const auto& dec : decors) draw_decor(w, dec);
    // Obstacles
    for (const auto& e : obstacles) draw_obstacle(w, e);
    // Horizontal intersection cars
    for (const auto& e : horiz_cars) draw_horiz_car(w, e);
    // NPCs
    for (const auto& e : npcs) draw_npc(w, e);

    // Road-side Traffic Light
    if (tlight.active) {
        int ty = (int)tlight.y;
        if (ty >= -2 && ty < dims_.rows + 2) {
            for (int i = 0; i < 3; i++) {
                int pty = ty + i;
                if (pty < 0 || pty >= dims_.rows) continue;
                int cp = CP_TLIGHT_OFF;
                if (i == 0 && tlight.state == 2) cp = CP_TLIGHT_RED;
                if (i == 1 && tlight.state == 1) cp = CP_TLIGHT_YEL;
                if (i == 2 && tlight.state == 0) cp = CP_TLIGHT_GRN;
                fill_block(w, pty, tlight.x, cp);
                fill_block(w, pty, tlight.x + 1, cp);
            }
        }
    }

    // Center-road Semaphore
    draw_semaphore(w);

    // Player last (on top)
    draw_player(w, horiz_dir);
}

// ─── Horizontal car drawing ───────────────────────────────────────────────────
void EntityManager::draw_horiz_car(WINDOW* w, const Entity& e) const {
    int py = (int)e.y;
    int px = e.x;
    int body_cp = (e.type == ENT_CAR_MAG) ? CP_NPC_MAG :
                  (e.type == ENT_CAR_CYN) ? CP_NPC_CYN : CP_NPC_ORG;

    auto safe_fill = [&](int r, int c, int cp) {
        if (r >= 0 && r < dims_.rows && c >= 0 && c < dims_.game_cols)
            fill_block(w, r, c, cp);
    };

    for (int dy = 0; dy < e.h; dy++) {
        int r = py + dy;
        for (int ddx = 0; ddx < e.w; ddx++) {
            int cx = px + ddx;
            int lead = (e.hspeed >= 0) ? 1 : e.w - 2;
            int cp   = (ddx == lead && dy == 0) ? CP_PLAYER_HL : body_cp;
            safe_fill(r, cx, cp);
        }
    }
}


bool EntityManager::player_collides(const ScreenDims& /*d*/) const {
    float px = player.x, py = player.y;
    int   pw = player.w, ph = player.h;

    for (const auto& e : npcs) {
        if (!e.active) continue;
        if (rects_overlap(py, (int)px, pw, ph, e.y, e.x, e.w, e.h))
            return true;
    }
    for (const auto& e : obstacles) {
        if (!e.active) continue;
        if (e.type == ENT_STOP_SIGN || e.type == ENT_YIELD_SIGN) continue; // signs don't block
        if (rects_overlap(py, (int)px, pw, ph, e.y, e.x, e.w, e.h))
            return true;
    }
    return false;
}

bool EntityManager::player_collides_horiz() const {
    float px = player.x, py = player.y;
    int   pw = player.w, ph = player.h;
    for (const auto& e : horiz_cars) {
        if (!e.active) continue;
        if (rects_overlap(py, (int)px, pw, ph, e.y, e.x, e.w, e.h))
            return true;
    }
    return false;
}

// ─── Callbacks ───────────────────────────────────────────────────────────────
void EntityManager::on_intersection_spawned(float y, bool traffic_light_is_green, int int_h) {
    // Signs spawn at intersection CORNERS (right side curb = right corner)
    // Left corner = curb_lx - 1, right corner = curb_rx + 1
    int right_corner_x = dims_.curb_rx + 1;
    int left_corner_x  = dims_.curb_lx - 1;
    if (left_corner_x < 0) left_corner_x = 0;

    int sign_type = rand() % 3; // 0=semaphore, 1=stop sign, 2=yield sign

    if (sign_type == 0) {
        // Place overhead semaphore in the middle of the road
        semaphore.y = y + int_h;
        semaphore.x = dims_.road_x + (dims_.lane_w + 1) * 2;
        semaphore.cycle_timer = 0.f;
        semaphore.state = 0; // Start GREEN
        semaphore.active = true;

        // Road-side traffic light at corner
        tlight.y = y + int_h;
        tlight.x = right_corner_x;
        tlight.cycle_timer = 0.f;
        tlight.state = 0;
        tlight.active = true;

    } else if (sign_type == 1) {
        // STOP sign at right corner
        Entity e;
        e.type = ENT_STOP_SIGN;
        e.y    = y + int_h;
        e.x    = right_corner_x;
        e.fx   = (float)e.x;
        e.w = 1; e.h = 1;
        e.speed = 0.f; e.hspeed = 0.f;
        e.active = true; e.lane = 3;
        e.npc_stopping = false;
        obstacles.push_back(e);

        // Also put a mirrored one at the left corner
        Entity e2 = e;
        e2.y  = y + int_h;
        e2.x  = left_corner_x;
        e2.fx = (float)e2.x;
        obstacles.push_back(e2);

        tlight.active    = false;
        semaphore.active = false;

    } else {
        // Yield sign at right corner
        Entity e;
        e.type = ENT_YIELD_SIGN;
        e.y    = y + int_h;
        e.x    = right_corner_x;
        e.fx   = (float)e.x;
        e.w = 1; e.h = 1;
        e.speed = 0.f; e.hspeed = 0.f;
        e.active = true; e.lane = 3;
        e.npc_stopping = false;
        obstacles.push_back(e);

        // Mirrored at left corner
        Entity e2 = e;
        e2.y  = y + int_h;
        e2.x  = left_corner_x;
        e2.fx = (float)e2.x;
        obstacles.push_back(e2);

        tlight.active    = false;
        semaphore.active = false;
    }

    // Police car NPC at the corner — moving slowly in traffic (not parked)
    if (rand() % 3 == 0) {
        Entity p;
        p.type   = ENT_POLICE;
        p.y      = y;
        // Randomly place in lane 2 or 3 (forward traffic)
        int police_lane = 2 + (rand() % 2);
        p.lane   = police_lane;
        p.x      = dims_.lane_cx[police_lane] - 1;
        p.fx     = (float)p.x;
        p.w = 2; p.h = 3;
        // Cruising at a moderate speed (police patrol pace)
        p.speed  = 50.f + rand() % 20;  // 50–70 km/h cruise
        p.hspeed = 0.f;
        p.active = true;
        p.npc_stopping = false;
        p.npc_orig_speed = p.speed;
        npcs.push_back(p);
    }

    // Spawn horizontal traffic
    spawn_horiz_cars(y, traffic_light_is_green);
}
