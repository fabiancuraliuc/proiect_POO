#define _XOPEN_SOURCE_EXTENDED 1
#include "ncurses_wrapper.h"
#include <stdexcept>
#include <cstring>

// ─── Colour initialisation ────────────────────────────────────────────────────
void ncurses_init() {
    setlocale(LC_ALL, "");
    initscr();

    if (!has_colors()) {
        endwin();
        throw std::runtime_error("Terminal does not support colours.");
    }

    start_color();
    use_default_colors();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    // Pick colour indices depending on 256-colour support
    int dark_gray  = (COLORS >= 256) ? 238 : COLOR_BLACK;
    int orange     = (COLORS >= 256) ? 202 : COLOR_YELLOW; // closest to orange
    int brown_fg   = (COLORS >= 256) ? 130 : COLOR_YELLOW;
    int brown_bg   = (COLORS >= 256) ? 94  : COLOR_BLACK;
    int lt_blue    = (COLORS >= 256) ? 33  : COLOR_BLUE;
    int lt_green   = (COLORS >= 256) ? 46  : COLOR_GREEN;
    int dk_green   = (COLORS >= 256) ? 22  : COLOR_GREEN;

    init_pair(CP_GRASS,      dk_green,      dk_green);
    init_pair(CP_WATER,      lt_blue,       lt_blue);
    init_pair(CP_TREE_TOP,   lt_green,      lt_green);
    init_pair(CP_TREE_TRNK,  brown_fg,      brown_bg);
    init_pair(CP_ROAD,       dark_gray,     dark_gray);
    init_pair(CP_MARK,       COLOR_WHITE,   dark_gray);
    init_pair(CP_CURB_W,     COLOR_BLACK,   COLOR_WHITE);
    init_pair(CP_CURB_B,     COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_PLAYER,     COLOR_RED,     COLOR_RED);
    init_pair(CP_PLAYER_HL,  COLOR_CYAN,    COLOR_CYAN);
    init_pair(CP_NPC_MAG,    COLOR_MAGENTA, COLOR_MAGENTA);
    init_pair(CP_NPC_CYN,    COLOR_CYAN,    COLOR_CYAN);
    init_pair(CP_NPC_ORG,    orange,        orange);
    init_pair(CP_NPC_RED,    COLOR_RED,     COLOR_RED);
    init_pair(CP_TRUCK,      COLOR_YELLOW,  COLOR_YELLOW);
    init_pair(CP_TRUCK_W,    dark_gray,     COLOR_YELLOW);
    init_pair(CP_CONE,       COLOR_YELLOW,  dark_gray);
    init_pair(CP_BARIC_R,    COLOR_WHITE,   COLOR_RED);
    init_pair(CP_BARIC_W,    COLOR_RED,     COLOR_WHITE);
    init_pair(CP_UI_BG,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_UI_SCORE,   COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_UI_PENAL,   COLOR_RED,     COLOR_BLACK);
    init_pair(CP_UI_SPEED,   COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_UI_FUEL,    COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_FUEL_FULL,  COLOR_BLACK,   COLOR_GREEN);
    init_pair(CP_FUEL_LOW,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(CP_FUEL_CRIT,  COLOR_WHITE,   COLOR_RED);
    init_pair(CP_FUEL_EMPTY, COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_RADAR,      COLOR_BLACK,   COLOR_YELLOW);
    init_pair(CP_GAMEOVER,   COLOR_WHITE,   COLOR_RED);
    init_pair(CP_MENU_HL,    COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_SPEED_WARN, COLOR_RED,     COLOR_BLACK);
    init_pair(CP_BORDER1,    COLOR_RED,     COLOR_BLACK);
    init_pair(CP_BORDER2,    COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_BORDER3,    COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_ROAD_STRIPE,COLOR_WHITE,   COLOR_RED);
    init_pair(CP_CENTER_LINE,COLOR_YELLOW,  dark_gray);
    init_pair(CP_TLIGHT_RED, COLOR_RED,     COLOR_RED);
    init_pair(CP_TLIGHT_YEL, COLOR_YELLOW,  COLOR_YELLOW);
    init_pair(CP_TLIGHT_GRN, COLOR_GREEN,   COLOR_GREEN);
    init_pair(CP_TLIGHT_OFF, COLOR_BLACK,   dark_gray);
    init_pair(CP_STOP_SIGN,  COLOR_WHITE,   COLOR_RED);   // white text on red octagon
    init_pair(CP_POLICE_BODY,COLOR_BLACK,   COLOR_WHITE);  // dark details on white body
    init_pair(CP_POLICE_WHT, COLOR_WHITE,   COLOR_WHITE);  // white solid
    init_pair(CP_POLICE_FLSH,COLOR_RED,     COLOR_BLUE);   // red-on-blue flash
    init_pair(CP_INTERSECTION,dark_gray,    dark_gray);

    // Environment & extra colours
    int house_wall1  = (COLORS >= 256) ? 167 : COLOR_RED;
    int house_roof   = (COLORS >= 256) ? 88  : COLOR_RED;
    int bld_gray     = (COLORS >= 256) ? 240 : COLOR_WHITE;
    int bld_win_bg   = (COLORS >= 256) ? 236 : COLOR_BLACK;
    int parking_bg   = (COLORS >= 256) ? 234 : COLOR_BLACK;
    int sign_blue    = (COLORS >= 256) ? 27  : COLOR_BLUE;

    init_pair(CP_HOUSE_WALL, COLOR_WHITE,   house_wall1);
    init_pair(CP_HOUSE_ROOF, COLOR_WHITE,   house_roof);
    init_pair(CP_BUILDING,   COLOR_WHITE,   bld_gray);
    init_pair(CP_BUILDING_W, COLOR_YELLOW,  bld_win_bg);
    init_pair(CP_PARKING,    COLOR_WHITE,   parking_bg);
    init_pair(CP_PARKING_LN, COLOR_WHITE,   dark_gray);
    init_pair(CP_YIELD_SIGN, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_PENALTY,    COLOR_RED,     COLOR_BLACK);
    init_pair(CP_SIGN_POST,  COLOR_WHITE,   sign_blue);
}

void ncurses_cleanup() {
    endwin();
}

// ─── Dimension calculation ────────────────────────────────────────────────────
ScreenDims compute_dims() {
    ScreenDims d{};
    getmaxyx(stdscr, d.rows, d.cols);

    d.ui_cols   = 22;
    d.ui_x      = d.cols - d.ui_cols;
    d.game_cols = d.ui_x;

    // Road geometry: 4 lanes of lane_w, 3 divider columns, 1-char curbs each side
    d.lane_w    = 6;
    int road_total = 4 * d.lane_w + 3;   // 4 lanes + 3 divider columns
    int curb_w  = 1;
    int side    = (d.game_cols - road_total - 2 * curb_w) / 2;
    if (side < 5) side = 5;

    d.grass_lw  = side;
    d.curb_lx   = side;
    d.road_x    = side + curb_w;
    d.curb_rx   = d.road_x + road_total;
    d.grass_rx  = d.curb_rx + curb_w;

    // Lane centres
    for (int i = 0; i < 4; i++) {
        d.lane_cx[i] = d.road_x + i * (d.lane_w + 1) + d.lane_w / 2;
    }

    d.player_start_x = d.lane_cx[2] - 1;   // first forward lane, 2-wide sprite
    d.player_start_y = d.rows - 5;

    // Horizontal geometry
    d.lane_h = 3;
    int horiz_road_total = 4 * d.lane_h + 3; // 4 lanes + 3 dividers
    int horiz_side = (d.rows - horiz_road_total - 2 * curb_w) / 2;
    if (horiz_side < 0) horiz_side = 0;
    
    d.horiz_curb_uy = horiz_side;
    d.horiz_road_y  = horiz_side + curb_w;
    d.horiz_curb_dy = d.horiz_road_y + horiz_road_total;
    
    for (int i = 0; i < 4; i++) {
        d.horiz_cy[i] = d.horiz_road_y + i * (d.lane_h + 1) + d.lane_h / 2;
    }

    return d;
}

// ─── Drawing primitives ───────────────────────────────────────────────────────
void fill_block(WINDOW* w, int y, int x, int cp) {
    int my, mx;
    getmaxyx(w, my, mx);
    if (y < 0 || y >= my || x < 0 || x >= mx) return;
    wattron(w, COLOR_PAIR(cp));
    mvwaddch(w, y, x, ' ');
    wattroff(w, COLOR_PAIR(cp));
}

void draw_wch(WINDOW* w, int y, int x, int cp, wchar_t ch) {
    cchar_t cc;
    wchar_t arr[2] = {ch, L'\0'};
    setcchar(&cc, arr, A_NORMAL, (short)cp, nullptr);
    mvwadd_wch(w, y, x, &cc);
}

void draw_str(WINDOW* w, int y, int x, int cp, const char* s, int attr) {
    wattron(w, COLOR_PAIR(cp) | attr);
    mvwprintw(w, y, x, "%s", s);
    wattroff(w, COLOR_PAIR(cp) | attr);
}

void draw_wstr(WINDOW* w, int y, int x, int cp, const wchar_t* s) {
    wattron(w, COLOR_PAIR(cp));
    mvwaddwstr(w, y, x, s);
    wattroff(w, COLOR_PAIR(cp));
}

void fill_rect(WINDOW* w, int y, int x, int h, int ww, int cp) {
    int my, mx;
    getmaxyx(w, my, mx);
    wattron(w, COLOR_PAIR(cp));
    for (int dy = 0; dy < h; dy++) {
        int ry = y + dy;
        if (ry < 0 || ry >= my) continue;
        for (int dx = 0; dx < ww; dx++) {
            int rx = x + dx;
            if (rx < 0 || rx >= mx) continue;
            mvwaddch(w, ry, rx, ' ');
        }
    }
    wattroff(w, COLOR_PAIR(cp));
}
