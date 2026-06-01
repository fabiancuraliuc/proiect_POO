#pragma once
#include "entity_manager.h"
#include <string>

// ─── Rules engine ─────────────────────────────────────────────────────────────
class RulesEngine {
public:
    // Called once per new game
    void reset();

    // Called every frame; returns penalty points added this frame (0 or more)
    // pen_msg is set to a Romanian description of the infraction if penalty > 0
    // bonus_score is incremented when player respects a sign
    int  update(float dt, int player_speed_kmh, const ScreenDims& d,
                EntityManager& ent, std::string& pen_msg, float& bonus_score);

    // Road-boundary check: returns -1 if player went off left, +1 if off right, 0 ok
    int  road_bounds_check(float player_x, int player_w, const ScreenDims& d) const;

private:
    float last_tlight_y_    = -999.f;
    float last_semaphore_y_ = -999.f;
    float last_stop_y_      = -999.f;
    float last_yield_y_     = -999.f;
    float last_police_y_    = -999.f;

    // For bonus tracking: did the player slow for signs?
    bool  rewarded_tlight_  = false;
    bool  rewarded_stop_    = false;
    bool  rewarded_yield_   = false;
};
