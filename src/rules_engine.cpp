#include "rules_engine.h"
#include <algorithm>
#include <cstring>

void RulesEngine::reset() {
    last_tlight_y_    = -999.f;
    last_semaphore_y_ = -999.f;
    last_stop_y_      = -999.f;
    last_yield_y_     = -999.f;
    last_police_y_    = -999.f;
    rewarded_tlight_  = false;
    rewarded_stop_    = false;
    rewarded_yield_   = false;
}

// ─── Per-frame update ─────────────────────────────────────────────────────────
int RulesEngine::update(float dt, int player_speed_kmh, const ScreenDims& d,
                        EntityManager& ent, std::string& pen_msg, float& bonus_score)
{
    (void)dt;
    (void)d;
    int penalty = 0;
    float py = ent.player.y;

    // ── Road-side Traffic Light (red) ────────────────────────────────────────
    if (ent.tlight.active && ent.tlight.state == 2) { // RED
        if (ent.tlight.y > py && last_tlight_y_ <= py) {
            penalty += 1;
            pen_msg = "!! SEMN ROSU - PENALIZARE !!";
            rewarded_tlight_ = false;
        }
    } else if (ent.tlight.active && ent.tlight.state == 0) { // GREEN
        // Bonus: passed green light at reasonable speed
        if (ent.tlight.y > py && last_tlight_y_ <= py && !rewarded_tlight_) {
            bonus_score += 50.f;
            rewarded_tlight_ = true;
        }
    }
    if (ent.tlight.active) last_tlight_y_ = ent.tlight.y;
    else { last_tlight_y_ = -999.f; rewarded_tlight_ = false; }

    // ── Center-road Semaphore ─────────────────────────────────────────────────
    if (ent.semaphore.active) {
        float sem_y = ent.semaphore.y;
        if (ent.semaphore.is_red()) {
            // Player passed a red semaphore → penalty
            if (sem_y > py && last_semaphore_y_ <= py) {
                penalty += 1;
                pen_msg = "!! SEMAFOR ROSU - PENALIZARE !!";
            }
        } else if (ent.semaphore.is_yellow()) {
            // Passed yellow without slowing → penalty
            if (sem_y > py && last_semaphore_y_ <= py && player_speed_kmh > 60) {
                penalty += 1;
                pen_msg = "!! SEMAFOR GALBEN - PENALIZARE !!";
            }
        } else {
            // GREEN: bonus for passing correctly
            if (sem_y > py && last_semaphore_y_ <= py) {
                bonus_score += 75.f;
            }
        }
        last_semaphore_y_ = sem_y;
    } else {
        last_semaphore_y_ = -999.f;
    }

    // ── Obstacles: STOP sign, YIELD sign, POLICE ─────────────────────────────
    bool found_stop   = false;
    bool found_yield  = false;
    bool found_police = false;

    for (const auto& e : ent.obstacles) {
        if (!e.active) continue;

        if (e.type == ENT_STOP_SIGN) {
            found_stop = true;
            if (e.y > py && last_stop_y_ <= py) {
                if (player_speed_kmh > 5) {
                    penalty += 1;
                    pen_msg = "!! STOP IGNORAT - PENALIZARE !!";
                    rewarded_stop_ = false;
                } else if (!rewarded_stop_) {
                    // Respected stop sign → bonus
                    bonus_score += 100.f;
                    rewarded_stop_ = true;
                }
            }
            last_stop_y_ = e.y;
        }

        if (e.type == ENT_YIELD_SIGN) {
            found_yield = true;
            if (e.y > py && last_yield_y_ <= py) {
                if (player_speed_kmh > 30) {
                    penalty += 1;
                    pen_msg = "!! CEDEAZA TRECEREA - PENALIZARE !!";
                    rewarded_yield_ = false;
                } else if (!rewarded_yield_) {
                    bonus_score += 50.f;
                    rewarded_yield_ = true;
                }
            }
            last_yield_y_ = e.y;
        }
    }

    // ── Police NPC: speeding past them → penalty ──────────────────────────────
    for (const auto& e : ent.npcs) {
        if (!e.active || e.type != ENT_POLICE) continue;
        found_police = true;
        if (e.y > py && last_police_y_ <= py) {
            if (player_speed_kmh > 90) {
                penalty += 1;
                pen_msg = "!! VITEZA 90+ LANGA POLITIE !!";
            }
        }
        last_police_y_ = e.y;
    }

    if (!found_stop)   { last_stop_y_   = -999.f; rewarded_stop_  = false; }
    if (!found_yield)  { last_yield_y_  = -999.f; rewarded_yield_ = false; }
    if (!found_police)   last_police_y_ = -999.f;

    return penalty;
}

// ─── Road boundary check ──────────────────────────────────────────────────────
int RulesEngine::road_bounds_check(float player_x, int player_w,
                                    const ScreenDims& d) const {
    if ((int)player_x < d.road_x)               return -1;
    if ((int)player_x + player_w > d.curb_rx)   return +1;
    return 0;
}
