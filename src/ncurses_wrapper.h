#pragma once
#define _XOPEN_SOURCE_EXTENDED 1
#include <ncursesw/ncurses.h>
#include <locale.h>

// ─── Color Pair IDs ──────────────────────────────────────────────────────────
enum ColorPair : short {
    CP_DEFAULT    = 0,
    CP_GRASS      = 1,   // green solid
    CP_WATER      = 2,   // blue solid
    CP_TREE_TOP   = 3,   // bright green solid (tree canopy)
    CP_TREE_TRNK  = 4,   // brown/dark trunk
    CP_ROAD       = 5,   // dark-gray solid (asphalt)
    CP_MARK       = 6,   // white on dark-gray (lane dash)
    CP_CURB_W     = 7,   // black-on-white curb stripe
    CP_CURB_B     = 8,   // white-on-black curb stripe
    CP_PLAYER     = 9,   // red solid (player body)
    CP_PLAYER_HL  = 10,  // cyan solid (headlights)
    CP_NPC_MAG    = 11,  // magenta solid
    CP_NPC_CYN    = 12,  // cyan solid (NPC)
    CP_NPC_ORG    = 13,  // orange (yellow-on-red approx, or 256c)
    CP_NPC_RED    = 14,  // red NPC (same hue, no headlights drawn)
    CP_TRUCK      = 15,  // yellow solid (truck body)
    CP_TRUCK_W    = 16,  // black-on-yellow (truck windshield)
    CP_CONE       = 17,  // yellow on dark-gray (traffic cone)
    CP_BARIC_R    = 18,  // white-on-red (barricade stripe A)
    CP_BARIC_W    = 19,  // red-on-white (barricade stripe B)
    CP_UI_BG      = 20,  // white-on-black (UI panel background text)
    CP_UI_SCORE   = 21,  // green-on-black
    CP_UI_PENAL   = 22,  // red-on-black
    CP_UI_SPEED   = 23,  // cyan-on-black
    CP_UI_FUEL    = 24,  // yellow-on-black
    CP_FUEL_FULL  = 25,  // black-on-green  (fuel bar high)
    CP_FUEL_LOW   = 26,  // black-on-yellow (fuel bar mid)
    CP_FUEL_CRIT  = 27,  // white-on-red    (fuel bar critical)
    CP_FUEL_EMPTY = 28,  // white-on-black  (empty segment)
    CP_RADAR      = 29,  // black-on-yellow (radar zone road stripe)
    CP_GAMEOVER   = 30,  // white-on-red    (Game Over overlay)
    CP_MENU_HL    = 31,  // black-on-cyan   (menu highlight)
    CP_SPEED_WARN = 32,  // red-on-black    (speeding warning flash)
    CP_BORDER1    = 33,  // red-on-black    (UI border colour 1)
    CP_BORDER2    = 34,  // yellow-on-black (UI border colour 2)
    CP_BORDER3    = 35,  // cyan-on-black   (UI border colour 3)
    CP_ROAD_STRIPE= 36,  // white-on-red    (speed-zone road stripe)
    CP_CENTER_LINE= 37,  // yellow-on-dark-gray (solid double yellow)
    CP_TLIGHT_RED = 38,  // red solid
    CP_TLIGHT_YEL = 39,  // yellow solid
    CP_TLIGHT_GRN = 40,  // green solid
    CP_TLIGHT_OFF = 41,  // black on dark gray
    CP_STOP_SIGN  = 42,  // red on white
    CP_POLICE_BODY= 43,  // blue solid
    CP_POLICE_WHT = 44,  // white solid
    CP_POLICE_FLSH= 45,  // red/blue flash
    CP_INTERSECTION=46,  // dark-gray solid for horizontal road
    CP_HOUSE_WALL = 47,  // house wall (various)
    CP_HOUSE_ROOF = 48,  // house roof (dark red/brown)
    CP_BUILDING   = 49,  // building (gray)
    CP_BUILDING_W = 50,  // building window (yellow on gray)
    CP_PARKING    = 51,  // parking (white on dark gray)
    CP_PARKING_LN = 52,  // parking line (white on asphalt)
    CP_YIELD_SIGN = 53,  // yield sign (yellow on dark)
    CP_PENALTY    = 54,  // red-on-black penalty flash
    CP_SIGN_POST  = 55,  // road sign post (white on blue)
    CP_COUNT      = 56
};

// ─── Screen Layout ────────────────────────────────────────────────────────────
struct ScreenDims {
    int rows, cols;        // total terminal size
    int game_cols;         // game area width
    int ui_x, ui_cols;     // UI panel start x and width

    // Road geometry (all in global x)
    int grass_lw;          // left grass width
    int curb_lx;           // left curb x  (1 char wide)
    int road_x;            // road start x (after curb)
    int lane_w;            // each lane width (chars)
    int curb_rx;           // right curb x
    int grass_rx;          // right grass start x

    int lane_cx[4];        // centre-x of each lane (0,1 = oncoming, 2,3 = forward)
    int player_start_x;    // initial player x (top-left of 2-wide sprite)
    int player_start_y;    // initial player y

    // Horizontal Road geometry (all in global y)
    int lane_h;            // each horizontal lane height (chars)
    int horiz_road_y;      // top row of the horizontal road
    int horiz_curb_uy;     // upper horizontal curb y
    int horiz_curb_dy;     // lower horizontal curb y
    int horiz_cy[4];       // centre-y of each horizontal lane
};

// ─── API ─────────────────────────────────────────────────────────────────────
void        ncurses_init();
void        ncurses_cleanup();
ScreenDims  compute_dims();

// Draw a single space cell with background colour (solid block)
void fill_block(WINDOW* w, int y, int x, int cp);

// Draw a single wide character (e.g. U+25B2 ▲) with colour pair cp
void draw_wch(WINDOW* w, int y, int x, int cp, wchar_t ch);

// Draw a narrow ASCII string with colour pair cp (optional extra attr)
void draw_str(WINDOW* w, int y, int x, int cp, const char* s, int attr = A_NORMAL);

// Draw a wide string
void draw_wstr(WINDOW* w, int y, int x, int cp, const wchar_t* s);

// Fill a rectangle with a colour (using spaces for solid block)
void fill_rect(WINDOW* w, int y, int x, int h, int ww, int cp);
