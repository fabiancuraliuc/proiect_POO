#pragma once
#include "entity_manager.h"
#include "rules_engine.h"
#include <chrono>
#include <string>

// ─── Game state enum ──────────────────────────────────────────────────────────
enum class GameState { MENU, PLAYING, GAMEOVER };

enum class IntersectionType { NONE, CROSS, T_LEFT, T_RIGHT, L_LEFT, L_RIGHT };
enum class Heading { NORTH, SOUTH, EAST, WEST };

// ─── Main game class ──────────────────────────────────────────────────────────
class Game {
public:
    Game();
    ~Game();
    void run();                  // blocking main loop

private:
    // ── State & logic ────────────────────────────────────────────────────────
    GameState     state_        = GameState::MENU;
    EntityManager ent_mgr_;
    RulesEngine   rules_;
    ScreenDims    dims_;

    float  score_           = 0.f;   // metres
    float  bonus_score_     = 0.f;   // bonus from respecting traffic signs
    int    penalties_       = 0;
    static const int MAX_PENALTIES = 5;  // game over threshold
    int    karma_           = 100;   // Law abiding score (0-100)
    
    // Physics - gear-based system
    float  player_speed_    = 0.f;   // km/h (actual physics speed)
    float  display_speed_   = 0.f;   // speed shown on dial (with inertia)
    float  engine_rpm_      = 800.f; // RPM for display
    int    current_gear_    = 1;     // 1-5 gears
    float  gear_shift_cd_   = 0.f;   // cooldown after shift
    float  clutch_factor_   = 1.f;   // 0-1, drops during shift
    float  brake_rate_      = 55.f;  // braking deceleration (km/h per sec)
    bool   accel_held_      = false;
    bool   brake_held_      = false;

    float  road_speed_      = 8.f;   // cells/second scroll
    float  scroll_offset_   = 0.f;   // fractional row offset for dash lines
    float  absolute_scroll_ = 0.f;   // absolute scroll distance for sparse markings
    float  game_time_       = 0.f;
    float  highscore_       = 0.f;

    // Intersection logic
    IntersectionType  pending_intersection_ = IntersectionType::NONE;
    float             intersection_y_       = -100.f;  // screen y (vert mode)
    float             intersection_x_       = -100.f;  // screen x (horiz mode)
    bool              at_intersection_      = false;
    Heading           heading_              = Heading::NORTH;
    float             intersection_timer_   = 0.f;
    float             next_intersection_    = 12.f;    // first vert intersection
    float             horiz_int_timer_      = 0.f;     // timer for horiz intersections
    float             next_horiz_int_       = 10.f;    // next horiz intersection in seconds

    // Horizontal driving mode (after turning at intersection)
    bool   driving_horiz_       = false;  // true while driving horizontally
    float  horiz_scroll_        = 0.f;   // fractional col offset for horiz dash
    int    horiz_lane_          = 2;     // player lane 0-3 on horizontal road
    float  player_horiz_y_      = 0.f;  // actual pixel-y of player in horiz mode
    float  player_horiz_target_ = 0.f;  // target y for smooth slide
    // heading_ encodes current direction (EAST/WEST = horiz, NORTH/SOUTH = vert)

    // Input smoothing timers to bypass OS key-repeat delay
    float  key_up_timer_    = 0.f;
    float  key_down_timer_  = 0.f;

    // Flash timer for speed-warn label
    float  warn_flash_      = 0.f;
    bool   warn_visible_    = false;

    // Penalty overlay message (shown in red on screen)
    std::string penalty_msg_;
    float       penalty_msg_timer_ = 0.f;

    // ── Windows ─────────────────────────────────────────────────────────────
    WINDOW* game_win_  = nullptr;
    WINDOW* ui_win_    = nullptr;

    // ── Timing ──────────────────────────────────────────────────────────────
    using Clock = std::chrono::steady_clock;
    using TP    = std::chrono::time_point<Clock>;
    TP last_frame_;

    // ── Private methods ──────────────────────────────────────────────────────
    void init_windows();
    void destroy_windows();
    void resize_check();

    float get_dt();

    // State handlers
    void handle_menu();
    void handle_playing();
    void handle_gameover();

    // Drawing
    void draw_background();       // grass, water, curbs
    void draw_road();             // vertical road - asphalt + lane markings
    void draw_road_horiz();       // horizontal driving road
    void draw_background_horiz(); // horizontal mode grass/curbs
    void draw_intersection();     // horizontal road overlay at intersection (vert mode)
    void draw_intersection_horiz(); // vertical road overlay at intersection (horiz mode)
    void draw_ui();              // right-side panel
    void draw_menu_overlay();
    void draw_gameover_overlay();

    // Road drawing helpers
    void draw_curb_cell(int y, int x, int scroll_int) const;
    void draw_dash_cell(int y, int x, int scroll_int) const;
    void draw_grass_row(int y) const;

    // UI helpers
    void draw_ui_border()                           const;
    void draw_ui_stat(int row, const wchar_t* icon, int icon_cp,
                      const char* label, const char* value, int val_cp) const;
    void draw_karma_bar(int row, int karma)          const;

    // Speed / scroll control
};
