#define _XOPEN_SOURCE_EXTENDED 1
#include "game_state.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <thread>

// ─── Timing constants ─────────────────────────────────────────────────────────
static const int   TARGET_FPS   = 30;
static const float FRAME_TIME   = 1.f / TARGET_FPS;

// Speed mapping: player_speed_ (km/h) -> road_speed_ (cells/sec)
// Slightly non-linear for better feel at high speeds
static float kmh_to_scroll(float kmh) {
    if (kmh <= 0.f) return 0.f;
    return kmh * 0.15f;  // purely linear: 60 km/h -> 9 cells/sec
}

// ─── Constructor / Destructor ────────────────────────────────────────────────
Game::Game() {
    ncurses_init();
    dims_ = compute_dims();
    init_windows();
    last_frame_ = Clock::now();
}

Game::~Game() {
    destroy_windows();
    ncurses_cleanup();
}

// ─── Window management ───────────────────────────────────────────────────────
void Game::init_windows() {
    dims_ = compute_dims();
    if (game_win_) { delwin(game_win_); game_win_ = nullptr; }
    if (ui_win_)   { delwin(ui_win_);   ui_win_   = nullptr; }

    game_win_ = newwin(dims_.rows, dims_.game_cols, 0, 0);
    ui_win_   = newwin(dims_.rows, dims_.ui_cols,   0, dims_.ui_x);

    keypad(game_win_, TRUE);
    keypad(stdscr,    TRUE);
    nodelay(game_win_, TRUE);
    nodelay(stdscr,    TRUE);
}

void Game::destroy_windows() {
    if (game_win_) { delwin(game_win_); game_win_ = nullptr; }
    if (ui_win_)   { delwin(ui_win_);   ui_win_   = nullptr; }
}

void Game::resize_check() {
    int r, c;
    getmaxyx(stdscr, r, c);
    if (r != dims_.rows || c != dims_.cols) {
        resizeterm(r, c);
        init_windows();
        ent_mgr_.reset(dims_);
    }
}

// ─── Delta time ──────────────────────────────────────────────────────────────
float Game::get_dt() {
    auto now = Clock::now();
    float dt = std::chrono::duration<float>(now - last_frame_).count();
    last_frame_ = now;
    // Clamp to avoid spiral of death AND micro-stutters from tiny dt
    return std::clamp(dt, 0.001f, 0.08f);
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void Game::run() {
    bool running = true;
    while (running) {
        resize_check();

        switch (state_) {
            case GameState::MENU:     handle_menu();     break;
            case GameState::PLAYING:  handle_playing();  break;
            case GameState::GAMEOVER: handle_gameover(); break;
        }

    // Enforce ~30 FPS — measure from AFTER last_frame_ was set in get_dt()
        std::this_thread::sleep_for(
            std::chrono::duration<float>(FRAME_TIME));
    }
}

// ─── MENU ────────────────────────────────────────────────────────────────────
void Game::handle_menu() {
    get_dt();   // drain timer
    draw_background();
    draw_road();
    draw_menu_overlay();
    wrefresh(game_win_);
    wrefresh(ui_win_);

    int key = wgetch(stdscr);
    if (key == '\n' || key == ' ' || key == KEY_ENTER) {
        // Start game
        score_        = 0.f;
        bonus_score_  = 0.f;
        penalties_    = 0;
        karma_        = 100;
        player_speed_ = 0.f;
        display_speed_ = 0.f;
        engine_rpm_   = 800.f;
        current_gear_ = 1;
        gear_shift_cd_ = 0.f;
        clutch_factor_ = 1.f;
        accel_held_   = false;
        brake_held_   = false;
        key_up_timer_   = 0.f;
        key_down_timer_ = 0.f;
        road_speed_   = kmh_to_scroll(0);
        scroll_offset_= 0.f;
        horiz_scroll_ = 0.f;
        game_time_    = 0.f;
        warn_flash_   = 0.f;
        warn_visible_ = false;
        driving_horiz_ = false;
        horiz_lane_   = 2;
        pending_intersection_ = IntersectionType::NONE;
        intersection_timer_ = 0.f;
        horiz_int_timer_ = 0.f;
        next_horiz_int_ = 10.f;
        heading_ = Heading::NORTH;
        ent_mgr_.init(dims_);
        rules_.reset();
        state_ = GameState::PLAYING;
        last_frame_ = Clock::now();
    } else if (key == 'q' || key == 'Q') {
        destroy_windows();
        ncurses_cleanup();
        std::exit(0);
    }
}

// ─── PLAYING ─────────────────────────────────────────────────────────────────
void Game::handle_playing() {
    float dt = get_dt();
    game_time_ += dt;

    // ── Input ────────────────────────────────────────────────────────────────
    // Key hold timers: stay "held" for KEY_HOLD_WINDOW seconds after last event
    static const float KEY_HOLD_WINDOW = 0.85f;

    int move_dir = 0;
    int key;
    while ((key = wgetch(stdscr)) != ERR) {
        switch (key) {
            case KEY_LEFT:  case 'a': case 'A':
                if (driving_horiz_) {
                    if (heading_ == Heading::WEST) key_up_timer_ = KEY_HOLD_WINDOW; // facing left -> A is forward
                    else key_down_timer_ = KEY_HOLD_WINDOW; // facing right -> A is backward (brake)
                }
                else move_dir = -1;
                break;
            case KEY_RIGHT: case 'd': case 'D':
                if (driving_horiz_) {
                    if (heading_ == Heading::WEST) key_down_timer_ = KEY_HOLD_WINDOW; // facing left -> D is backward (brake)
                    else key_up_timer_ = KEY_HOLD_WINDOW; // facing right -> D is forward
                }
                else move_dir = +1;
                break;
            case KEY_UP:    case 'w': case 'W':
                if (!driving_horiz_) key_up_timer_   = KEY_HOLD_WINDOW; // W = accel
                else                 move_dir = -1;                      // W = lane up in horiz
                break;
            case KEY_DOWN:  case 's': case 'S':
                if (!driving_horiz_) key_down_timer_ = KEY_HOLD_WINDOW; // S = brake
                else                 move_dir = +1;                      // S = lane down in horiz
                break;
            case 'p': case 'P':
                // Simple pause: wait for next keypress
                draw_background(); draw_road();
                draw_str(game_win_, dims_.rows/2, dims_.game_cols/2 - 4,
                         CP_GAMEOVER, " PAUSED ", A_BOLD);
                wrefresh(game_win_);
                nodelay(stdscr, FALSE);
                wgetch(stdscr);
                nodelay(stdscr, TRUE);
                last_frame_ = Clock::now();
                return;
            case 'q': case 'Q':
                destroy_windows();
                ncurses_cleanup();
                std::exit(0);
        }
    }

    // Drain timers and derive held state
    key_up_timer_   = std::max(0.f, key_up_timer_   - dt);
    key_down_timer_ = std::max(0.f, key_down_timer_ - dt);

    // If we changed lanes, artificially extend the acceleration timer
    // to mask ncurses autorepeat cancellation on simultaneous keypress.
    if (move_dir != 0) {
        if (key_up_timer_ > 0.05f) key_up_timer_ = KEY_HOLD_WINDOW;
        if (key_down_timer_ > 0.05f) key_down_timer_ = KEY_HOLD_WINDOW;
    }

    accel_held_ = (key_up_timer_   > 0.f);
    brake_held_ = (key_down_timer_ > 0.f);

    // ── Physics (Gear-based Acceleration) ─────────────────────────────────
    // Gear ratios: max speed each gear can reach (km/h)
    static const float gear_max[6] = {0.f, 35.f, 65.f, 100.f, 140.f, 180.f};
    // Gear torque multiplier (lower gear = more torque)
    static const float gear_torque[6] = {0.f, 1.8f, 1.4f, 1.1f, 0.85f, 0.65f};

    // Shift cooldown
    if (gear_shift_cd_ > 0.f) {
        gear_shift_cd_ -= dt;
        clutch_factor_ = std::max(0.5f, 1.f - (gear_shift_cd_ / 0.3f)); // slight power dip
        if (gear_shift_cd_ <= 0.f) {
            gear_shift_cd_ = 0.f;
            clutch_factor_ = 1.f;
        }
    }

    // Auto-shift logic
    if (accel_held_ && current_gear_ < 5 && player_speed_ >= gear_max[current_gear_] * 0.92f) {
        current_gear_++;
        gear_shift_cd_ = 0.3f; // 300ms shift delay
    }
    if (player_speed_ < gear_max[std::max(1, current_gear_ - 1)] * 0.5f && current_gear_ > 1) {
        current_gear_--;
        gear_shift_cd_ = 0.2f;
    }

    if (accel_held_) {
        // Torque-based acceleration: high torque at low gear, less at high
        float torque = gear_torque[current_gear_] * clutch_factor_;
        // Diminishing returns near gear's max speed
        float gear_headroom = 1.f - (player_speed_ / gear_max[current_gear_]);
        gear_headroom = std::clamp(gear_headroom, 0.05f, 1.f);
        float accel = 40.f * torque * gear_headroom;
        player_speed_ = std::min(player_speed_ + accel * dt, 180.f);
    } else if (brake_held_) {
        // ABS-style braking: more effective at higher speeds
        float brake_eff = brake_rate_ * (0.6f + 0.4f * std::min(player_speed_ / 80.f, 1.f));
        player_speed_ = std::max(player_speed_ - brake_eff * dt, 0.f);
    } else {
        // Engine braking + rolling resistance + air drag
        float engine_brake = 5.f * gear_torque[current_gear_] * 0.3f;
        float rolling = 2.f;
        float aero = player_speed_ * player_speed_ * 0.0004f;
        float total_drag = engine_brake + rolling + aero;
        player_speed_ = std::max(player_speed_ - total_drag * dt, 0.f);
    }

    // RPM: linear climb/fall tied to speed within current gear
    float gear_min_spd = (current_gear_ > 1) ? gear_max[current_gear_ - 1] * 0.5f : 0.f;
    float gear_range   = gear_max[current_gear_] - gear_min_spd;
    float rpm_pct      = (gear_range > 0.f) ?
                         (player_speed_ - gear_min_spd) / gear_range : 0.f;
    rpm_pct = std::clamp(rpm_pct, 0.f, 1.f);
    float target_rpm = 800.f + rpm_pct * 6200.f; // idle 800 → redline 7000

    // Linear RPM movement: fixed RPM/sec rate (not exponential)
    float rpm_rate = 4500.f * dt;   // climb speed: ~4500 RPM/s
    if (accel_held_ && target_rpm > engine_rpm_)
        engine_rpm_ = std::min(engine_rpm_ + rpm_rate, target_rpm);
    else if (brake_held_ || target_rpm < engine_rpm_)
        engine_rpm_ = std::max(engine_rpm_ - rpm_rate * 1.8f, target_rpm); // drop faster
    else
        engine_rpm_ = engine_rpm_ + (target_rpm - engine_rpm_) * 3.f * dt; // idle settle

    // Display speed with inertia (needle lag)
    float speed_diff = player_speed_ - display_speed_;
    float lag = (std::abs(speed_diff) > 20.f) ? 6.f : 3.f; // faster catch-up for big changes
    display_speed_ += speed_diff * lag * dt;

    // ── Road scroll ──────────────────────────────────────────────────
    road_speed_    = kmh_to_scroll(player_speed_);
    absolute_scroll_ += road_speed_ * dt;
    if (!driving_horiz_) {
        scroll_offset_ = fmod(scroll_offset_ + road_speed_ * dt, 6.f);
        ent_mgr_.player.w = 2;
        ent_mgr_.player.h = 3;
    } else {
        if (heading_ == Heading::WEST) {
            horiz_scroll_ = fmod(horiz_scroll_ - road_speed_ * dt + 60.f, 6.f);
        } else {
            horiz_scroll_ = fmod(horiz_scroll_ + road_speed_ * dt, 6.f);
        }
        // Calculate vertical road boundaries for horizontal driving

        // In horiz mode: lane change = move_dir adjusts player y
        if (move_dir != 0) {
            int new_lane = horiz_lane_ + move_dir;
            if (new_lane >= 0 && new_lane <= 3) {
                horiz_lane_ = new_lane;
                // player y is centered in the horizontal lane
                player_horiz_target_ = (float)(dims_.horiz_cy[horiz_lane_] - ent_mgr_.player.h / 2);
            }
        }
        // Smooth slide on Y axis
        float dy = player_horiz_target_ - player_horiz_y_;
        float slide = std::min(std::abs(dy), 18.f * dt);
        player_horiz_y_ += (dy > 0 ? slide : -slide);
        // Mirror player y onto entity manager player y
        ent_mgr_.player.y = player_horiz_y_;
        ent_mgr_.player.target_x = ent_mgr_.player.x; // keep x fixed
        ent_mgr_.player.w = 4;
        ent_mgr_.player.h = 2;
    }

    // ── Intersection logic (VERTICAL MODE) ──────────────────────────────
    if (!driving_horiz_) {
        intersection_timer_ += dt;
        if (pending_intersection_ == IntersectionType::NONE && intersection_timer_ > next_intersection_) {
            int type_rand = rand() % 5;
            pending_intersection_ = static_cast<IntersectionType>(type_rand + 1);
            intersection_y_ = -10.f;
            intersection_timer_ = 0.f;
            next_intersection_ = 5.f + (rand() % 10); // next in 5-15s
            bool cur_green = ent_mgr_.is_traffic_light_green();
            ent_mgr_.on_intersection_spawned(intersection_y_, cur_green);
        }

        if (pending_intersection_ != IntersectionType::NONE) {
            intersection_y_ += road_speed_ * dt;
            float player_y = ent_mgr_.player.y;
            at_intersection_ = (std::abs(player_y - intersection_y_) < 4.f);

            if (intersection_y_ > dims_.rows + 5) {
                pending_intersection_ = IntersectionType::NONE;
                at_intersection_ = false;
            }

            if (at_intersection_ && move_dir != 0) {
                bool can_turn = false;
                bool penalize = false;
                if (move_dir == -1 && (pending_intersection_ == IntersectionType::CROSS || 
                                       pending_intersection_ == IntersectionType::T_LEFT ||
                                       pending_intersection_ == IntersectionType::L_LEFT)) {
                    can_turn = true;
                    if (!ent_mgr_.is_traffic_light_green()) penalize = true;
                }
                if (move_dir == +1 && (pending_intersection_ == IntersectionType::CROSS || 
                                       pending_intersection_ == IntersectionType::T_RIGHT ||
                                       pending_intersection_ == IntersectionType::L_RIGHT)) {
                    can_turn = true;
                }

                if (can_turn) {
                    if (penalize) {
                        warn_flash_ = 1.5f; warn_visible_ = true;
                        karma_ = std::max(0, karma_ - 20);
                        penalties_++;
                        penalty_msg_ = "!! ROSU - PENALIZARE !!";
                        penalty_msg_timer_ = 2.0f;
                    }
                    // Update heading
                    if (move_dir == -1) {
                        heading_ = (heading_ == Heading::NORTH) ? Heading::WEST :
                                   (heading_ == Heading::WEST) ? Heading::SOUTH :
                                   (heading_ == Heading::SOUTH) ? Heading::EAST : Heading::NORTH;
                    } else {
                        heading_ = (heading_ == Heading::NORTH) ? Heading::EAST :
                                   (heading_ == Heading::EAST) ? Heading::SOUTH :
                                   (heading_ == Heading::SOUTH) ? Heading::WEST : Heading::NORTH;
                    }
                    // Enter horizontal mode if heading is EAST or WEST
                    if (heading_ == Heading::EAST || heading_ == Heading::WEST) {
                        driving_horiz_ = true;
                        horiz_lane_    = 2;  // start in forward lane
                        player_horiz_y_      = (float)(dims_.horiz_cy[horiz_lane_] - ent_mgr_.player.h / 2);
                        player_horiz_target_ = player_horiz_y_;
                        scroll_offset_ = 0.f;
                        horiz_scroll_  = 0.f;
                        
                        if (heading_ == Heading::EAST) {
                            ent_mgr_.player.x = (float)(dims_.game_cols * 0.2f);
                        } else {
                            ent_mgr_.player.x = (float)(dims_.game_cols * 0.7f);
                        }
                        ent_mgr_.player.target_x = ent_mgr_.player.x;
                    } else {
                        driving_horiz_ = false;
                    }
                    // Reset key timers so vertical hold doesn't bleed into horiz mode
                    key_up_timer_   = 0.f;
                    key_down_timer_ = 0.f;
                    ent_mgr_.set_horiz(driving_horiz_);
                    ent_mgr_.reset(dims_);
                    pending_intersection_ = IntersectionType::NONE;
                    at_intersection_ = false;
                    intersection_timer_ = 0.f;
                    horiz_int_timer_ = 0.f; // reset horiz timer on mode switch
                    move_dir = 0;
                }
            }
        }
    }

    // ── Intersection logic (HORIZONTAL MODE) ─────────────────────────────
    if (driving_horiz_) {
        horiz_int_timer_ += dt;
        if (pending_intersection_ == IntersectionType::NONE && horiz_int_timer_ > next_horiz_int_) {
            // Spawn a vertical intersection crossing the horizontal road
            int type_rand = rand() % 3; // CROSS, T_LEFT, T_RIGHT
            IntersectionType types[] = {IntersectionType::CROSS, IntersectionType::T_LEFT, IntersectionType::T_RIGHT};
            pending_intersection_ = types[type_rand];
            horiz_int_timer_ = 0.f;
            next_horiz_int_ = 6.f + (rand() % 8); // next in 6-14s
            // intersection_x_ tracks the horizontal position
            if (heading_ == Heading::EAST) {
                intersection_x_ = (float)(dims_.game_cols + 10);
            } else {
                intersection_x_ = -10.f;
            }
        }

        if (pending_intersection_ != IntersectionType::NONE) {
            // Scroll intersection towards player
            if (heading_ == Heading::EAST) {
                intersection_x_ -= road_speed_ * dt;
            } else {
                intersection_x_ += road_speed_ * dt;
            }
            float player_x = ent_mgr_.player.x;
            at_intersection_ = (std::abs(player_x - intersection_x_) < 6.f);

            // Off-screen cleanup
            if ((heading_ == Heading::EAST && intersection_x_ < -20.f) ||
                (heading_ == Heading::WEST && intersection_x_ > dims_.game_cols + 20.f)) {
                pending_intersection_ = IntersectionType::NONE;
                at_intersection_ = false;
            }

            // Turn: W = up (turn North from East, or South from West), S = down
            if (at_intersection_ && move_dir != 0) {
                bool can_turn = true;
                if (can_turn) {
                    if (move_dir == -1) {
                        heading_ = (heading_ == Heading::EAST) ? Heading::NORTH :
                                   (heading_ == Heading::WEST) ? Heading::SOUTH : heading_;
                    } else {
                        heading_ = (heading_ == Heading::EAST) ? Heading::SOUTH :
                                   (heading_ == Heading::WEST) ? Heading::NORTH : heading_;
                    }
                    // Go back to vertical mode
                    driving_horiz_ = false;
                    ent_mgr_.set_horiz(false);
                    scroll_offset_ = 0.f;
                    horiz_scroll_  = 0.f;
                    // Reset key timers
                    key_up_timer_   = 0.f;
                    key_down_timer_ = 0.f;
                    ent_mgr_.reset(dims_);
                    pending_intersection_ = IntersectionType::NONE;
                    at_intersection_ = false;
                    intersection_timer_ = 0.f;
                    horiz_int_timer_ = 0.f;
                    move_dir = 0;
                }
            }
        }
    }

    // ── Rules engine ─────────────────────────────────────────────────────
    if (!driving_horiz_) {
        std::string pen_msg;
        int pen = rules_.update(dt, (int)player_speed_, dims_, ent_mgr_, pen_msg, bonus_score_);
        if (pen > 0) {
            penalties_ += pen;
            warn_flash_ = 1.5f;
            warn_visible_ = true;
            karma_ = std::max(0, karma_ - 15);
            penalty_msg_ = pen_msg;
            penalty_msg_timer_ = 2.5f;
        }
    }

    // Warn flash timer
    if (warn_flash_ > 0.f) {
        warn_flash_ -= dt;
        warn_visible_ = (int)(warn_flash_ * 4) % 2 == 0;
        if (warn_flash_ <= 0.f) { warn_flash_ = 0.f; warn_visible_ = false; }
    }
    // Penalty message timer
    if (penalty_msg_timer_ > 0.f) {
        penalty_msg_timer_ -= dt;
        if (penalty_msg_timer_ <= 0.f) { penalty_msg_timer_ = 0.f; penalty_msg_.clear(); }
    }

    // ── Entity update ───────────────────────────────────────────────────────
    // In horiz mode, move_dir was used for lane changes, pass 0 to entity manager
    int ent_move = driving_horiz_ ? 0 : move_dir;
    ent_mgr_.update(dt, player_speed_, ent_move, dims_, (int)heading_);

    // ── Clamp player to road ────────────────────────────────────────────
    if (!driving_horiz_) {
        int bound = rules_.road_bounds_check(ent_mgr_.player.x,
                                             ent_mgr_.player.w, dims_);
        if (bound == -1) {
            ent_mgr_.player.x = (float)dims_.road_x;
            ent_mgr_.player.target_x = ent_mgr_.player.x;
        } else if (bound == +1) {
            ent_mgr_.player.x = (float)(dims_.curb_rx - ent_mgr_.player.w);
            ent_mgr_.player.target_x = ent_mgr_.player.x;
        }
    }

    // ── Score ─────────────────────────────────────────────────────────────────
    score_ += road_speed_ * dt * 2.5f;   // ~metres

    // ── Game Over conditions ──────────────────────────────────────────────────────
    // Horizontal car collision at red light = penalty
    static bool horiz_collision_last_ = false;
    bool horiz_hit = ent_mgr_.player_collides_horiz();
    if (horiz_hit && !horiz_collision_last_) {
        penalties_++;
        karma_ = std::max(0, karma_ - 30);
        warn_flash_ = 2.0f;
        warn_visible_ = true;
        penalty_msg_ = "!! ACCIDENT LA SEMAFOR - PENALIZARE !!";
        penalty_msg_timer_ = 2.5f;
    }
    horiz_collision_last_ = horiz_hit;

    // ── Police speed check: >90 km/h near police = penalty ──────────────────
    static float police_penalty_cd_ = 0.f;  // cooldown so it doesn't fire every frame
    police_penalty_cd_ = std::max(0.f, police_penalty_cd_ - dt);
    if (police_penalty_cd_ <= 0.f && player_speed_ > 90.f &&
        ent_mgr_.any_police_on_screen()) {
        penalties_++;
        karma_ = std::max(0, karma_ - 20);
        warn_flash_ = 1.8f;
        warn_visible_ = true;
        penalty_msg_ = "!! POLITIE! VITEZA PESTE 90 - PENALIZARE !!";
        penalty_msg_timer_ = 2.5f;
        police_penalty_cd_ = 4.0f;  // can only re-trigger after 4 s
    }

    // 5 penalties = game over; direct collision also ends game
    if (ent_mgr_.player_collides(dims_) || penalties_ >= MAX_PENALTIES) {
        if (score_ + bonus_score_ > highscore_) highscore_ = score_ + bonus_score_;
        state_ = GameState::GAMEOVER;
    }

    // ── Draw ──────────────────────────────────────────────────────────────────
    if (driving_horiz_) {
        draw_background_horiz();
        draw_road_horiz();
        draw_intersection_horiz();  // draw vertical road crossing in horiz mode
    } else {
        draw_background();
        draw_road();
        draw_intersection();
    }
    int horiz_dir = 0;
    if (driving_horiz_) {
        horiz_dir = (heading_ == Heading::EAST) ? 1 : -1;
    }
    ent_mgr_.draw(game_win_, (int)scroll_offset_, dims_, horiz_dir);

    // ── Penalty overlay (red, center screen) ─────────────────────────────────
    if (!penalty_msg_.empty() && penalty_msg_timer_ > 0.f) {
        int flash_on = (int)(penalty_msg_timer_ * 5) % 2 == 0;
        if (flash_on) {
            int msg_len = (int)penalty_msg_.size();
            int msg_x = (dims_.game_cols - msg_len) / 2;
            int msg_y = dims_.rows / 2 - 2;
            // Shadow box
            fill_rect(game_win_, msg_y - 1, msg_x - 2, 3, msg_len + 4, CP_UI_BG);
            // Red bold text
            draw_str(game_win_, msg_y, msg_x, CP_PENALTY, penalty_msg_.c_str(), A_BOLD);
            // Karma info
            char k_buf[32];
            std::snprintf(k_buf, sizeof(k_buf), "Karma: %d", karma_);
            draw_str(game_win_, msg_y + 1, (dims_.game_cols - 10) / 2, CP_UI_PENAL, k_buf, A_BOLD);
        }
    }

    draw_ui();
    wrefresh(game_win_);
    wrefresh(ui_win_);
}

// ─── GAME OVER ───────────────────────────────────────────────────────────────
void Game::handle_gameover() {
    get_dt();
    draw_background();
    draw_road();
    ent_mgr_.draw(game_win_, (int)scroll_offset_, dims_);
    draw_ui();
    draw_gameover_overlay();
    wrefresh(game_win_);
    wrefresh(ui_win_);

    int key = wgetch(stdscr);
    if (key == 'r' || key == 'R' || key == '\n' || key == ' ') {
        score_        = 0.f;
        bonus_score_  = 0.f;
        penalties_    = 0;
        karma_        = 100;
        player_speed_ = 0.f;
        display_speed_ = 0.f;
        engine_rpm_   = 800.f;
        current_gear_ = 1;
        gear_shift_cd_ = 0.f;
        clutch_factor_ = 1.f;
        accel_held_   = false;
        brake_held_   = false;
        key_up_timer_   = 0.f;
        key_down_timer_ = 0.f;
        road_speed_   = kmh_to_scroll(0);
        scroll_offset_= 0.f;
        absolute_scroll_ = 0.f;
        horiz_scroll_ = 0.f;
        game_time_    = 0.f;
        warn_flash_   = 0.f;
        warn_visible_ = false;
        driving_horiz_ = false;
        horiz_lane_   = 2;
        pending_intersection_ = IntersectionType::NONE;
        intersection_timer_ = 0.f;
        horiz_int_timer_ = 0.f;
        next_horiz_int_ = 10.f;
        heading_ = Heading::NORTH;
        ent_mgr_.set_horiz(false);
        ent_mgr_.reset(dims_);
        rules_.reset();
        state_ = GameState::PLAYING;
        last_frame_ = Clock::now();
    } else if (key == 'm' || key == 'M') {
        state_ = GameState::MENU;
    } else if (key == 'q' || key == 'Q') {
        destroy_windows();
        ncurses_cleanup();
        std::exit(0);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// DRAWING
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Curb cell: alternating black/white stripes based on scroll ───────────────
void Game::draw_curb_cell(int y, int x, int scroll_int) const {
    if (y < 0 || y >= dims_.rows || x < 0 || x >= dims_.game_cols) return;
    int cp = (((y + scroll_int) % 2 + 2) % 2 == 0) ? CP_CURB_W : CP_CURB_B;
    fill_block(game_win_, y, x, cp);
}

// ─── Lane-dash cell: white dash every 3 rows, gap every 3 rows ───────────────
void Game::draw_dash_cell(int y, int x, int scroll_int) const {
    if (y < 0 || y >= dims_.rows || x < 0 || x >= dims_.game_cols) return;
    int phase = ((y + scroll_int) % 6 + 6) % 6;  // ensure positive modulo
    int cp    = (phase < 3) ? CP_MARK : CP_ROAD;
    fill_block(game_win_, y, x, cp);
}

// ─── Grass row ───────────────────────────────────────────────────────────────
void Game::draw_grass_row(int y) const {
    // Left grass
    for (int x = 0; x < dims_.curb_lx; x++)
        fill_block(game_win_, y, x, CP_GRASS);
    // Right grass (from curb_rx+1 to game_cols-1)
    for (int x = dims_.grass_rx; x < dims_.game_cols; x++)
        fill_block(game_win_, y, x, CP_GRASS);
}

// ─── Background (grass + curbs) ───────────────────────────────────────────────
void Game::draw_background() {
    werase(game_win_);
    int si = (int)scroll_offset_;

    for (int y = 0; y < dims_.rows; y++) {
        // Left grass
        for (int x = 0; x < dims_.curb_lx; x++)
            fill_block(game_win_, y, x, CP_GRASS);

        // Left curb
        draw_curb_cell(y, dims_.curb_lx, si);

        // Right curb
        draw_curb_cell(y, dims_.curb_rx, si);

        // Right grass
        for (int x = dims_.grass_rx; x < dims_.game_cols; x++)
            fill_block(game_win_, y, x, CP_GRASS);
    }
}

// ─── Vertical driving: road surface ───────────────────────────────────────────
namespace {
    const char* digit_font[10][5] = {
        {" ### ", "#   #", "#   #", "#   #", " ### "}, // 0
        {"  #  ", " ##  ", "  #  ", "  #  ", " ### "}, // 1
        {" ### ", "    #", " ### ", "#    ", " ### "}, // 2
        {" ### ", "    #", " ### ", "    #", " ### "}, // 3
        {"#   #", "#   #", " ### ", "    #", "    #"}, // 4
        {" ### ", "#    ", " ### ", "    #", " ### "}, // 5
        {" ### ", "#    ", " ### ", "#   #", " ### "}, // 6
        {" ### ", "    #", "   # ", "  #  ", "  #  "}, // 7
        {" ### ", "#   #", " ### ", "#   #", " ### "}, // 8
        {" ### ", "#   #", " ### ", "    #", " ### "}  // 9
    };

    void draw_big_number(WINDOW* w, int y, int x, int num) {
        if (num < 0 || num > 99) return;
        int d1 = num / 10;
        int d2 = num % 10;
        for (int r = 0; r < 5; r++) {
            for (int c = 0; c < 5; c++) {
                if (digit_font[d1][r][c] == '#') fill_block(w, y + r, x + c, CP_MARK);
                if (digit_font[d2][r][c] == '#') fill_block(w, y + r, x + 6 + c, CP_MARK);
            }
        }
    }
}

// Lane layout (4 lanes, lane_w=6, 1-char dividers):
//  [lane0][D0][lane1][CENTER_LEFT][CENTER_RIGHT][lane2][D2][lane3]
//  D0 = white dash (same dir, lanes 0-1 = oncoming)
//  CENTER = solid double yellow (between oncoming & forward)
//  D2 = white dash (same dir, lanes 2-3 = forward)
//  Edge = first/last road column = solid white line
void Game::draw_road() {
    int si = (int)scroll_offset_;

    // Precompute divider x positions (dividers at relative positions lane_w, 2*lane_w+1, 3*lane_w+2)
    // With lane_total = lane_w+1 = 7:
    //   div0 at rel = lane_w      (after lane0)  -> same-dir dash (between 0-1, both oncoming)
    //   div1 at rel = 2*lane_w+1  (after lane1)  -> SOLID YELLOW center (between 1-2)
    //   div2 at rel = 3*lane_w+2  (after lane2)  -> same-dir dash (between 2-3, both forward)
    int lane_total = dims_.lane_w + 1;
    int div0 = dims_.road_x + dims_.lane_w;               // after lane0
    int div1 = dims_.road_x + dims_.lane_w + lane_total;  // between lane1 and lane2 (center)
    int div2 = dims_.road_x + dims_.lane_w + 2*lane_total;// after lane2

    // Edge lines: first and last road columns
    int edge_l = dims_.road_x;           // leftmost road col
    int edge_r = dims_.curb_rx - 1;      // rightmost road col

    for (int y = 0; y < dims_.rows; y++) {
        for (int x = dims_.road_x; x < dims_.curb_rx; x++) {
            if (x == edge_l || x == edge_r) {
                // Solid white edge line
                fill_block(game_win_, y, x, CP_MARK);
            } else if (x == div0) {
                // Dashed white: oncoming lanes divider
                draw_dash_cell(y, x, si);
            } else if (x == div1) {
                // Solid yellow center line
                fill_block(game_win_, y, x, CP_CENTER_LINE);
            } else if (x == div2) {
                // Dashed white: forward lanes divider
                draw_dash_cell(y, x, si);
            } else {
                fill_block(game_win_, y, x, CP_ROAD);
            }
        }
    }

    // Draw speed limit painted on road
    float scroll_len = 200.f;
    int limit_idx = ((int)(absolute_scroll_ / scroll_len)) % 4;
    int limits[4] = {50, 70, 90, 70};
    int current_limit = limits[limit_idx];

    int speed_y = (int)fmod(absolute_scroll_, scroll_len);
    if (speed_y >= 0 && speed_y < dims_.rows) {
        int cx = dims_.lane_cx[2] - 3;
        draw_big_number(game_win_, speed_y, cx, current_limit);
    }
}

// ─── Horizontal driving: background ───────────────────────────────────────────
// In horiz mode the road scrolls horizontally, grass is above/below the road.
void Game::draw_background_horiz() {
    werase(game_win_);
    int si = (int)horiz_scroll_;
    int curb_uy = dims_.horiz_curb_uy;
    int curb_dy = dims_.horiz_curb_dy;

    for (int y = 0; y < dims_.rows; y++) {
        for (int x = 0; x < dims_.game_cols; x++) {
            if (y < curb_uy) {
                fill_block(game_win_, y, x, CP_GRASS);
            } else if (y == curb_uy) {
                // Top curb (scrolls on x)
                int cp = ((x + si) % 2 == 0) ? CP_CURB_W : CP_CURB_B;
                fill_block(game_win_, y, x, cp);
            } else if (y == curb_dy) {
                // Bottom curb
                int cp = ((x + si) % 2 == 0) ? CP_CURB_W : CP_CURB_B;
                fill_block(game_win_, y, x, cp);
            } else if (y > curb_dy) {
                fill_block(game_win_, y, x, CP_GRASS);
            }
            // road interior drawn by draw_road_horiz()
        }
    }
}

// ─── Horizontal driving: road surface with rotated lane markings ────────────────
void Game::draw_road_horiz() {
    int si = (int)horiz_scroll_;
    int road_y = dims_.horiz_road_y;
    int lane_total = dims_.lane_h + 1;

    // Divider row positions
    int div0 = road_y + dims_.lane_h;
    int div1 = road_y + dims_.lane_h + lane_total;
    int div2 = road_y + dims_.lane_h + 2 * lane_total;
    int edge_u = road_y;
    int edge_d = dims_.horiz_curb_dy - 1;

    for (int y = road_y; y <= edge_d; y++) {
        for (int x = 0; x < dims_.game_cols; x++) {
            if (y == edge_u || y == edge_d) {
                fill_block(game_win_, y, x, CP_MARK);
            } else if (y == div0) {
                int phase = (x + si) % 6;
                fill_block(game_win_, y, x, (phase < 3) ? CP_MARK : CP_ROAD);
            } else if (y == div1) {
                fill_block(game_win_, y, x, CP_CENTER_LINE);
            } else if (y == div2) {
                int phase = (x + si) % 6;
                fill_block(game_win_, y, x, (phase < 3) ? CP_MARK : CP_ROAD);
            } else {
                fill_block(game_win_, y, x, CP_ROAD);
            }
        }
    }

    // Speed marks on horizontal road
    float scroll_len = 200.f;
    int limit_idx = ((int)(absolute_scroll_ / scroll_len)) % 4;
    int limits[4] = {50, 70, 90, 70};
    int current_limit = limits[limit_idx];

    int speed_x = dims_.game_cols - (int)fmod(absolute_scroll_, scroll_len);
    if (heading_ == Heading::WEST) speed_x = (int)fmod(absolute_scroll_, scroll_len);

    if (speed_x >= -11 && speed_x < dims_.game_cols) {
        int cy = dims_.horiz_cy[2] - 2; // centers 5-row font
        draw_big_number(game_win_, cy, speed_x, current_limit);
    }

    // Compass indicator in top-left of road area
    const char* dir_str = (heading_ == Heading::EAST) ? ">> EAST" : "<< WEST";
    draw_str(game_win_, 1, 2, CP_UI_SPEED, dir_str, A_BOLD);
}



// ─── Intersection surface ──────────────────────────────────────────────────────
void Game::draw_intersection() {
    if (pending_intersection_ == IntersectionType::NONE) return;
    int iy = (int)intersection_y_;
    int h = 6; // height of the intersection
    if (iy + h < 0 || iy >= dims_.rows) return;

    for (int y = std::max(0, iy); y < std::min(dims_.rows, iy + h); y++) {
        if (pending_intersection_ == IntersectionType::CROSS || 
            pending_intersection_ == IntersectionType::T_LEFT ||
            pending_intersection_ == IntersectionType::L_LEFT) {
            for (int x = 0; x < dims_.road_x; x++) {
                fill_block(game_win_, y, x, CP_INTERSECTION);
            }
        }
        if (pending_intersection_ == IntersectionType::CROSS || 
            pending_intersection_ == IntersectionType::T_RIGHT ||
            pending_intersection_ == IntersectionType::L_RIGHT) {
            for (int x = dims_.curb_rx; x < dims_.game_cols; x++) {
                fill_block(game_win_, y, x, CP_INTERSECTION);
            }
        }
    }
}

// ─── Horizontal mode: vertical intersection overlay ─────────────────────────
void Game::draw_intersection_horiz() {
    if (!driving_horiz_ || pending_intersection_ == IntersectionType::NONE) return;
    int ix = (int)intersection_x_;
    int w = 6; // width of the vertical intersection arm
    if (ix + w < 0 || ix >= dims_.game_cols) return;

    int curb_uy = dims_.horiz_curb_uy;
    int curb_dy = dims_.horiz_curb_dy;

    for (int x = std::max(0, ix); x < std::min(dims_.game_cols, ix + w); x++) {
        // Draw vertical road crossing above the horizontal road
        if (pending_intersection_ == IntersectionType::CROSS ||
            pending_intersection_ == IntersectionType::T_LEFT) {
            for (int y = 0; y <= curb_uy; y++) {
                if (y >= 0 && y < dims_.rows)
                    fill_block(game_win_, y, x, CP_INTERSECTION);
            }
        }
        // Draw vertical road crossing below the horizontal road
        if (pending_intersection_ == IntersectionType::CROSS ||
            pending_intersection_ == IntersectionType::T_RIGHT) {
            for (int y = curb_dy; y < dims_.rows; y++) {
                if (y >= 0 && y < dims_.rows)
                    fill_block(game_win_, y, x, CP_INTERSECTION);
            }
        }
    }

    // Draw a "TURN" hint near the intersection if player is close
    if (at_intersection_) {
        int hint_y = curb_uy - 1;
        if (hint_y >= 0 && hint_y < dims_.rows && ix >= 0 && ix + 4 < dims_.game_cols) {
            draw_str(game_win_, hint_y, ix, CP_UI_SPEED, "TURN", A_BOLD);
        }
    }
}

// ─── UI panel (right side) ───────────────────────────────────────────────────
void Game::draw_ui() {
    werase(ui_win_);
    int w = dims_.ui_cols;

    // Black background
    fill_rect(ui_win_, 0, 0, dims_.rows, w, CP_UI_BG);

    // Multi-colour border
    draw_ui_border();

    // ── Stats section (top half) ─────────────────────────────────────────────
    // Title
    draw_str(ui_win_, 1, 2, CP_UI_SPEED, " DASHBOARD ", A_BOLD);

    // Separator
    draw_str(ui_win_, 2, 1, CP_BORDER2, "────────────────────", 0);

    // Score   ◎ symbol
    char score_buf[32];
    std::snprintf(score_buf, sizeof(score_buf), "%d m", (int)score_);
    draw_wstr(ui_win_, 4, 2, CP_UI_SCORE, L"\u25CE Score:");
    draw_str (ui_win_, 5, 4, CP_UI_SCORE, score_buf, A_BOLD);

    // Penalties  ⬡ symbol  (X/5)
    char pen_buf[32];
    std::snprintf(pen_buf, sizeof(pen_buf), "%d / %d", penalties_, MAX_PENALTIES);
    draw_wstr(ui_win_, 7, 2, CP_UI_PENAL, L"\u2B21 Penalties:");
    draw_str (ui_win_, 8, 4, penalties_ >= MAX_PENALTIES - 1 ? CP_GAMEOVER : CP_UI_PENAL, pen_buf, A_BOLD);

    // Bonus score from sign compliance
    char bon_buf[24];
    std::snprintf(bon_buf, sizeof(bon_buf), "+%d pts", (int)bonus_score_);
    draw_str(ui_win_, 9, 4, CP_UI_SCORE, bon_buf, 0);

    // Speed + Gear indicator
    draw_str(ui_win_, 11, 2, CP_UI_SPEED, "Speed:", 0);
    int bar_w = dims_.ui_cols - 4;
    float spd_pct = std::clamp(display_speed_ / 180.f, 0.f, 1.f);
    int filled = (int)(spd_pct * bar_w);
    for (int i = 0; i < bar_w; i++) {
        int cp = (i < bar_w * 0.33f) ? CP_FUEL_FULL :
                 (i < bar_w * 0.66f) ? CP_FUEL_LOW : CP_FUEL_CRIT;
        int ch = (i < filled) ? ACS_CKBOARD : ACS_BOARD;
        if (i >= filled) cp = CP_FUEL_EMPTY;
        wattron(ui_win_, COLOR_PAIR(cp));
        mvwaddch(ui_win_, 12, 2 + i, ch);
        wattroff(ui_win_, COLOR_PAIR(cp));
    }

    char spd_buf[24];
    std::snprintf(spd_buf, sizeof(spd_buf), "%3d km/h", (int)display_speed_);
    draw_str(ui_win_, 13, 4, CP_UI_SPEED, spd_buf, A_BOLD);

    // Gear & RPM
    char gear_buf[24];
    std::snprintf(gear_buf, sizeof(gear_buf), "Gear: %d  RPM:%4d", current_gear_, (int)engine_rpm_);
    draw_str(ui_win_, 14, 2, CP_UI_FUEL, gear_buf, 0);

    // Karma (Law Abiding Score)
    draw_wstr(ui_win_, 16, 2, CP_UI_FUEL, L"\u2696 Karma:");
    draw_karma_bar(17, karma_);

    // Separator
    draw_str(ui_win_, 19, 1, CP_BORDER3, "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80", 0);

    // Controls hint
    draw_str(ui_win_, 21, 2, CP_UI_BG, "<> Lane/Turn", 0);
    draw_str(ui_win_, 22, 2, CP_UI_BG, "^v Speed", 0);
    draw_str(ui_win_, 23, 2, CP_UI_BG, "P=Pause Q=Quit", 0);

    // Highscore at bottom
    if (highscore_ > 0.f) {
        char hi_buf[32];
        std::snprintf(hi_buf, sizeof(hi_buf), "Best:%dm", (int)highscore_);
        draw_str(ui_win_, dims_.rows - 2, 2, CP_UI_SCORE, hi_buf, A_BOLD);
    }
}

// ─── Multi-colour UI border ───────────────────────────────────────────────────
void Game::draw_ui_border() const {
    int w  = dims_.ui_cols;
    int h  = dims_.rows;
    int cp_cycle[4] = { CP_BORDER1, CP_BORDER2, CP_BORDER3, CP_UI_SPEED };

    for (int y = 0; y < h; y++) {
        int cp = cp_cycle[(y / 4) % 4];
        wattron(ui_win_, COLOR_PAIR(cp) | A_BOLD);
        mvwaddch(ui_win_, y, 0,   ACS_VLINE);
        mvwaddch(ui_win_, y, w-1, ACS_VLINE);
        wattroff(ui_win_, COLOR_PAIR(cp) | A_BOLD);
    }
    // Top/bottom horizontal borders
    wattron(ui_win_, COLOR_PAIR(CP_BORDER1) | A_BOLD);
    mvwhline(ui_win_, 0,   1, ACS_HLINE, w-2);
    mvwaddch(ui_win_, 0,   0,   ACS_ULCORNER);
    mvwaddch(ui_win_, 0,   w-1, ACS_URCORNER);
    wattroff(ui_win_, COLOR_PAIR(CP_BORDER1) | A_BOLD);

    wattron(ui_win_, COLOR_PAIR(CP_BORDER3) | A_BOLD);
    mvwhline(ui_win_, h-1, 1, ACS_HLINE, w-2);
    mvwaddch(ui_win_, h-1, 0,   ACS_LLCORNER);
    mvwaddch(ui_win_, h-1, w-1, ACS_LRCORNER);
    wattroff(ui_win_, COLOR_PAIR(CP_BORDER3) | A_BOLD);
}

// ─── Animated Karma bar ───────────────────────────────────────────────────────
void Game::draw_karma_bar(int row, int karma) const {
    float pct = std::clamp(karma / 100.f, 0.f, 1.f);
    int bar_w   = dims_.ui_cols - 4;
    int filled  = (int)(pct * bar_w);
    int cp_fill = (pct > 0.6f) ? CP_FUEL_FULL :
                  (pct > 0.3f)? CP_FUEL_LOW  : CP_FUEL_CRIT;

    wattron(ui_win_, COLOR_PAIR(cp_fill));
    for (int i = 0; i < filled; i++)
        mvwaddch(ui_win_, row, 2 + i, ' ');
    wattroff(ui_win_, COLOR_PAIR(cp_fill));

    wattron(ui_win_, COLOR_PAIR(CP_FUEL_EMPTY));
    for (int i = filled; i < bar_w; i++)
        mvwaddch(ui_win_, row, 2 + i, ACS_BOARD);
    wattroff(ui_win_, COLOR_PAIR(CP_FUEL_EMPTY));

    char pct_buf[8];
    std::snprintf(pct_buf, sizeof(pct_buf), "%3d", karma);
    draw_str(ui_win_, row, dims_.ui_cols - 5, CP_UI_FUEL, pct_buf, A_BOLD);
}

// ─── Menu overlay ────────────────────────────────────────────────────────────
void Game::draw_menu_overlay() {
    // Center box on screen
    int box_w = 32;
    int box_h = 12;
    int bx = (dims_.game_cols - box_w) / 2;
    int by = (dims_.rows - box_h) / 2;

    // Shadow box
    fill_rect(game_win_, by, bx, box_h, box_w, CP_UI_BG);

    // Title centered
    const char* title   = "=  TRAFFIC ARCADE  =";
    const char* sub     = "Block-Pixel Simulator";
    const char* sep     = "──────────────────────────────";
    const char* start   = "[ ENTER ] Start";
    const char* steer   = "Arrow Keys = Steer / Speed";
    const char* turn    = "At intersection: <- / ->  Turn";
    const char* pause_q = "P = Pause     Q = Quit";

    auto center_x = [&](const char* s) {
        return bx + (box_w - (int)strlen(s)) / 2;
    };

    draw_str(game_win_, by + 1, center_x(title),  CP_UI_SPEED, title,  A_BOLD);
    draw_str(game_win_, by + 2, center_x(sub),    CP_BORDER2,  sub,    0);
    draw_str(game_win_, by + 3, bx,               CP_BORDER3,  sep,    0);
    draw_str(game_win_, by + 5, center_x(start),  CP_UI_SCORE, start,  A_BOLD);
    draw_str(game_win_, by + 6, center_x(steer),  CP_UI_BG,    steer,  0);
    draw_str(game_win_, by + 7, center_x(turn),   CP_UI_BG,    turn,   0);
    draw_str(game_win_, by + 8, center_x(pause_q),CP_UI_BG,    pause_q,0);

    if (highscore_ > 0.f) {
        char hi_buf[48];
        std::snprintf(hi_buf, sizeof(hi_buf), "Best: %d m", (int)highscore_);
        draw_str(game_win_, by + 10, center_x(hi_buf), CP_UI_FUEL, hi_buf, A_BOLD);
    }
}

// ─── Game Over overlay ────────────────────────────────────────────────────────
void Game::draw_gameover_overlay() {
    int cx = dims_.game_cols / 2;
    int cy = dims_.rows / 2;

    fill_rect(game_win_, cy-4, cx-13, 9, 26, CP_GAMEOVER);

    draw_str(game_win_, cy-3, cx-8, CP_GAMEOVER, " GAME OVER ", A_BOLD);

    char sc_buf[40];
    std::snprintf(sc_buf, sizeof(sc_buf), " Score: %d m", (int)score_);
    draw_str(game_win_, cy-2, cx-9, CP_GAMEOVER, sc_buf, A_BOLD);

    char bon_buf[40];
    std::snprintf(bon_buf, sizeof(bon_buf), " Bonus:  +%d pts", (int)bonus_score_);
    draw_str(game_win_, cy-1, cx-9, CP_GAMEOVER, bon_buf, A_BOLD);

    char pen_buf[40];
    std::snprintf(pen_buf, sizeof(pen_buf), " Penalties: %d / %d", penalties_, MAX_PENALTIES);
    draw_str(game_win_, cy,   cx-9, CP_GAMEOVER, pen_buf, 0);

    if (score_ + bonus_score_ >= highscore_) {
        draw_str(game_win_, cy+1, cx-9, CP_GAMEOVER, " ★ New Best! ★", A_BOLD);
    }

    draw_str(game_win_, cy+2, cx-9, CP_GAMEOVER,
             " R=Restart  M=Menu  Q=Quit", 0);
}
