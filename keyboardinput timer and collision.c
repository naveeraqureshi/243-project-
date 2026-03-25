#include <stdlib.h>

/* ── Memory-mapped addresses ─────────────────────────────── */
#define VGA_BASE    0xFF203020
#define PIXEL_BASE  0x08000000
#define PS2_BASE    0xFF200100

volatile int   *vga_ctrl  = (volatile int   *) VGA_BASE;
volatile short *pixel_buf = (volatile short *) PIXEL_BASE;
volatile int   *ps2_ptr   = (volatile int   *) PS2_BASE;

/* ════════════════════════════════════════════════════════════
 * RGB565 COLOUR PALETTE
 * ════════════════════════════════════════════════════════════ */
#define BLACK        0x0000
#define WHITE        0xFFFF
#define MID_GREY     0x8410
#define LIGHT_GREY   0xC618

#define WALL_GREEN   0x3D07
#define FLOOR_LIGHT  0xF6EE
#define FLOOR_DARK   0xE5C7
#define FLOOR_FOOD   0x9B26
#define SKY_TAN      0xFFF3
#define HUD_BG       0x0000

#define STALL_BLUE   0x659F
#define STALL_HIGHLIGHT 0xFFE0  /* yellow highlight for active animal stall */
#define FENCE_BROWN  0xAB60
#define FENCE_DARK   0x8A40
#define STALL_SHELF  0xDCCA

#define BANNER_BG    0xFCF3
#define BANNER_BDR   0xD0B5
#define TITLE_COL    0xD0B5

#define BOX_GREY     0xB5B6
#define BOX_SEL      0x2D26

/* Cat */
#define C_ORANGE     0xFCA0
#define C_DARK_ORG   0xF340
#define C_LIGHT_ORG  0xFDC8
#define C_PINK       0xFBB2
#define C_GREEN      0x8660
#define C_NOSE       0xF8AB

/* Dog */
#define D_GOLD       0xF6C0
#define D_DARK       0xD500
#define D_NOSE       0x4228
#define D_TONGUE     0xFBB5
#define D_IRIS       0x8220

/* Bird */
#define B_BODY       0x8FD3
#define B_HEAD       0xCFF3
#define B_WING       0x2ABF
#define B_BEAK       0xFDC0
#define B_CHEEK      0xAD9F
#define B_DARK       0x4EAA

/* Hamster */
#define H_BODY       0xFED3
#define H_DARK       0xFCB0
#define H_PINK       0xFCB2
#define H_IRIS       0x0000

/* Rabbit */
#define R_WHITE      0xF7FF
#define R_PINK       0xFBB5
#define R_EYE        0x4C9F

/* Human */
#define P_SKIN       0xFDB8
#define P_HAIR       0xA340
#define P_SHIRT      0x657F
#define P_PANTS      0x4B5D
#define P_SHOE       0x4228

/* Food */
#define F_FISH_B     0x8D96
#define F_FISH_L     0xAEFF
#define F_BONE       0xF7DF
#define F_CARROT     0xFC80
#define F_LEAF       0x4DA0
#define F_SEED_L     0xECA0
#define F_SEED_D     0xA540

/* ════════════════════════════════════════════════════════════
 * PS/2 SCAN CODES
 * ════════════════════════════════════════════════════════════ */
#define PS2_BREAK    0xF0
#define PS2_EXT      0xE0
#define SC_UP        0x75
#define SC_DOWN      0x72
#define SC_LEFT      0x6B
#define SC_RIGHT     0x74
#define SC_SPACE     0x29
#define SC_ENTER     0x5A

#define MOVE_STEP    3

/* ── Player movement bounds ──────────────────────────────────
 * The player is restricted to the yellow food area (FLOOR_FOOD).
 * Horizontally: stay inside the green walls (x 28..292).
 * Vertically:   top edge = just below the fence bar at y~107
 *               bottom edge = bottom of screen minus feet
 * The fence/name strip sits at y=105..115; we keep the player
 * feet (cy+24) above 190 (the FLOOR_FOOD top) and the player
 * head (cy-23) below 115 so they cannot enter the stall area.
 * ─────────────────────────────────────────────────────────── */
#define PLAYER_X_MIN  28
#define PLAYER_X_MAX 292
#define PLAYER_Y_MIN 115   /* just below the fence/name bar          */
#define PLAYER_Y_MAX 182   /* feet stay on the food floor            */

/* ════════════════════════════════════════════════════════════
 * GAME LOGIC CONSTANTS
 *
 * Food IDs:   0=fish  1=bone  2=seeds  3=carrot   -1=none
 * Animal IDs: 0=cat   1=dog   2=bird   3=hamster   4=rabbit
 * ════════════════════════════════════════════════════════════ */
#define FOOD_NONE   -1
#define FOOD_FISH    0
#define FOOD_BONE    1
#define FOOD_SEEDS   2
#define FOOD_CARROT  3

/* Food box zones */
#define F0_X   61
#define F1_X  126
#define F2_X  191
#define F3_X  256
#define FOOD_Y 210

/* Pet stall centres (x only; collision now happens via food drop-off
 * at the bottom fence — player approaches from below) */
#define PET_Y   72
#define P_CX_0  46
#define P_CX_1 102
#define P_CX_2 158
#define P_CX_3 214
#define P_CX_4 270

/* Stall geometry (needed to draw highlight ring) */
#define STALL_TOP  20
#define STALL_BOT  105
#define STALL_W    56
#define POST_W     5

/* Drop-off zone: player stands near the fence (top of food area)
 * aligned with the animal column.
 * Player head is at cy-23; when cy == PLAYER_Y_MIN (115) the head
 * is at y=92, well inside the stall.  We trigger delivery when
 * player_y <= PLAYER_Y_MIN + 4 = 119 (i.e. standing at the fence).
 */
#define DROPOFF_Y_MAX  119   /* player_y must be <= this to deliver */

/* Correct food for each animal */
static const int correct_food[5] = {
    FOOD_FISH,    /* 0 = cat    */
    FOOD_BONE,    /* 1 = dog    */
    FOOD_SEEDS,   /* 2 = bird   */
    FOOD_SEEDS,   /* 3 = hamster*/
    FOOD_CARROT   /* 4 = rabbit */
};

/* Animal column centres (same order as animal IDs) */
static const int animal_cx[5] = { P_CX_0, P_CX_1, P_CX_2, P_CX_3, P_CX_4 };

/* ════════════════════════════════════════════════════════════
 * GAME STATE ENUM
 * ════════════════════════════════════════════════════════════ */
#define STATE_START  0
#define STATE_PLAY   1
#define STATE_END    2

/* ════════════════════════════════════════════════════════════
 * SIMPLE LCG RANDOM
 * ════════════════════════════════════════════════════════════ */
static unsigned int rng_seed = 12345;

unsigned int rand_next(void)
{
    rng_seed = rng_seed * 1664525u + 1013904223u;
    return rng_seed;
}

/* Returns a value in [0, n-1] */
int rand_range(int n)
{
    return (int)((rand_next() >> 16) % (unsigned int)n);
}

/* ════════════════════════════════════════════════════════════
 * PRIMITIVES
 * ════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short color)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    pixel_buf[y * 512 + x] = color;
}

void fill_rect(int x, int y, int w, int h, short color)
{
    int i, j;
    for (j = y; j < y+h; j++)
        for (i = x; i < x+w; i++)
            plot_pixel(i, j, color);
}

void draw_rect_border(int x, int y, int w, int h, short color)
{
    int i;
    for (i = x; i < x+w; i++) { plot_pixel(i, y, color); plot_pixel(i, y+h-1, color); }
    for (i = y; i < y+h; i++) { plot_pixel(x, i, color); plot_pixel(x+w-1, i, color); }
}

void draw_line(int x0, int y0, int x1, int y1, short color)
{
    int dx = x1-x0, dy = y1-y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx-dy;
    while (1) {
        plot_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void fill_circle(int cx, int cy, int rx, int ry, short color)
{
    int x, y;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x*x*ry*ry + y*y*rx*rx <= rx*rx*ry*ry)
                plot_pixel(cx+x, cy+y, color);
}

void fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, short color)
{
    int minx=x0, maxx=x0, miny=y0, maxy=y0;
    if (x1 < minx) minx=x1; if (x1 > maxx) maxx=x1;
    if (x2 < minx) minx=x2; if (x2 > maxx) maxx=x2;
    if (y1 < miny) miny=y1; if (y1 > maxy) maxy=y1;
    if (y2 < miny) miny=y2; if (y2 > maxy) maxy=y2;
    int x, y;
    for (y = miny; y <= maxy; y++)
    for (x = minx; x <= maxx; x++) {
        int d0 = (x1-x0)*(y-y0) - (x-x0)*(y1-y0);
        int d1 = (x2-x1)*(y-y1) - (x-x1)*(y2-y1);
        int d2 = (x0-x2)*(y-y2) - (x-x2)*(y0-y2);
        if ((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0))
            plot_pixel(x, y, color);
    }
}

/* ════════════════════════════════════════════════════════════
 * PIXEL FONT
 * ════════════════════════════════════════════════════════════ */
void draw_char(int x, int y, char ch, short c)
{
    switch(ch) {
    case 'P':
        fill_rect(x,   y,   2, 7, c);
        fill_rect(x+2, y,   3, 1, c); fill_rect(x+2, y+3, 3, 1, c);
        fill_rect(x+5, y+1, 1, 2, c);
        break;
    case 'E':
        fill_rect(x,   y,   2, 7, c);
        fill_rect(x+2, y,   3, 1, c); fill_rect(x+2, y+3, 2, 1, c);
        fill_rect(x+2, y+6, 3, 1, c);
        break;
    case 'T':
        fill_rect(x,   y,   5, 1, c);
        fill_rect(x+2, y+1, 1, 6, c);
        break;
    case 'C':
        fill_rect(x+1, y,   4, 1, c); fill_rect(x+1, y+6, 4, 1, c);
        fill_rect(x,   y+1, 1, 5, c);
        break;
    case 'A':
        fill_rect(x+1, y,   3, 1, c);
        fill_rect(x,   y+1, 1, 6, c); fill_rect(x+4, y+1, 1, 6, c);
        fill_rect(x+1, y+3, 3, 1, c);
        break;
    case 'F':
        fill_rect(x,   y,   2, 7, c);
        fill_rect(x+2, y,   3, 1, c); fill_rect(x+2, y+3, 2, 1, c);
        break;
    case 'G':
        fill_rect(x+1, y,   4, 1, c); fill_rect(x+1, y+6, 4, 1, c);
        fill_rect(x,   y+1, 1, 5, c);
        fill_rect(x+3, y+3, 2, 1, c); fill_rect(x+4, y+4, 1, 2, c);
        break;
    case 'M':
        fill_rect(x,   y,   1, 7, c); fill_rect(x+5, y,   1, 7, c);
        fill_rect(x+1, y+1, 1, 1, c); fill_rect(x+4, y+1, 1, 1, c);
        fill_rect(x+2, y+2, 1, 1, c); fill_rect(x+3, y+2, 1, 1, c);
        break;
    case 'D':
        fill_rect(x,   y,   1, 7, c);
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+1, y+6, 3, 1, c);
        fill_rect(x+4, y+1, 1, 5, c);
        break;
    case 'O':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+1, y+6, 3, 1, c);
        fill_rect(x,   y+1, 1, 5, c); fill_rect(x+4, y+1, 1, 5, c);
        break;
    case 'B':
        fill_rect(x,   y,   1, 7, c);
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+1, y+3, 3, 1, c);
        fill_rect(x+1, y+6, 3, 1, c);
        fill_rect(x+4, y+1, 1, 2, c); fill_rect(x+4, y+4, 1, 2, c);
        break;
    case 'I':
        fill_rect(x,   y,   5, 1, c);
        fill_rect(x,   y+6, 5, 1, c);
        fill_rect(x+2, y+1, 1, 5, c);
        break;
    case 'R':
        fill_rect(x,   y,   1, 7, c);
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+1, y+3, 3, 1, c);
        fill_rect(x+4, y+1, 1, 2, c);
        fill_rect(x+2, y+4, 1, 1, c); fill_rect(x+3, y+5, 1, 2, c);
        break;
    case 'H':
        fill_rect(x,   y,   1, 7, c); fill_rect(x+4, y,   1, 7, c);
        fill_rect(x+1, y+3, 3, 1, c);
        break;
    case 'S':
        fill_rect(x+1, y,   4, 1, c);
        fill_rect(x,   y+1, 1, 2, c);
        fill_rect(x+1, y+3, 3, 1, c);
        fill_rect(x+4, y+4, 1, 2, c);
        fill_rect(x,   y+6, 4, 1, c); fill_rect(x+4, y+6, 1, 1, c);
        break;
    case 'N':
        fill_rect(x,   y,   1, 7, c); fill_rect(x+4, y,   1, 7, c);
        fill_rect(x+1, y+1, 1, 1, c);
        fill_rect(x+2, y+2, 1, 2, c);
        fill_rect(x+3, y+4, 1, 1, c);
        break;
    case 'K':
        fill_rect(x,   y,   1, 7, c);
        fill_rect(x+3, y,   2, 2, c);
        fill_rect(x+1, y+3, 2, 1, c);
        fill_rect(x+3, y+4, 2, 3, c);
        break;
    case 'W':
        fill_rect(x,   y,   1, 7, c); fill_rect(x+5, y,   1, 7, c);
        fill_rect(x+1, y+5, 1, 1, c); fill_rect(x+4, y+5, 1, 1, c);
        fill_rect(x+2, y+5, 2, 2, c);
        break;
    case 'Y':
        fill_rect(x,   y,   1, 3, c); fill_rect(x+4, y,   1, 3, c);
        fill_rect(x+2, y+3, 1, 4, c);
        fill_rect(x+1, y+2, 1, 1, c); fill_rect(x+3, y+2, 1, 1, c);
        break;
    case 'U':
        fill_rect(x,   y,   1, 6, c); fill_rect(x+4, y,   1, 6, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case 'L':
        fill_rect(x,   y,   1, 7, c);
        fill_rect(x+1, y+6, 4, 1, c);
        break;
    case 'X':
        fill_rect(x,   y,   1, 2, c); fill_rect(x+4, y,   1, 2, c);
        fill_rect(x+1, y+2, 1, 1, c); fill_rect(x+3, y+2, 1, 1, c);
        fill_rect(x+2, y+3, 1, 1, c);
        fill_rect(x+1, y+4, 1, 1, c); fill_rect(x+3, y+4, 1, 1, c);
        fill_rect(x,   y+5, 1, 2, c); fill_rect(x+4, y+5, 1, 2, c);
        break;
    case '!':
        fill_rect(x+2, y,   1, 5, c);
        fill_rect(x+2, y+6, 1, 1, c);
        break;
    case ':':
        fill_rect(x+2, y+1, 1, 1, c);
        fill_rect(x+2, y+4, 1, 1, c);
        break;
    case '0':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+1, y+6, 3, 1, c);
        fill_rect(x,   y+1, 1, 5, c); fill_rect(x+4, y+1, 1, 5, c);
        fill_rect(x+3, y+2, 1, 1, c); fill_rect(x+2, y+4, 1, 1, c);
        break;
    case '1':
        fill_rect(x+1, y,   1, 1, c);
        fill_rect(x+2, y,   1, 7, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case '2':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+4, y+1, 1, 2, c);
        fill_rect(x+1, y+3, 3, 1, c); fill_rect(x,   y+4, 1, 2, c);
        fill_rect(x+1, y+6, 4, 1, c);
        break;
    case '3':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x+4, y+1, 1, 2, c);
        fill_rect(x+1, y+3, 3, 1, c); fill_rect(x+4, y+4, 1, 2, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case '4':
        fill_rect(x,   y,   1, 4, c); fill_rect(x+4, y,   1, 7, c);
        fill_rect(x+1, y+3, 3, 1, c);
        break;
    case '5':
        fill_rect(x,   y,   5, 1, c); fill_rect(x,   y+1, 1, 2, c);
        fill_rect(x+1, y+3, 3, 1, c); fill_rect(x+4, y+4, 1, 2, c);
        fill_rect(x,   y+6, 4, 1, c);
        break;
    case '6':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x,   y+1, 1, 5, c);
        fill_rect(x+1, y+3, 3, 1, c); fill_rect(x+4, y+4, 1, 2, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case '7':
        fill_rect(x,   y,   5, 1, c); fill_rect(x+4, y+1, 1, 2, c);
        fill_rect(x+3, y+3, 1, 2, c); fill_rect(x+2, y+5, 1, 2, c);
        break;
    case '8':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x,   y+1, 1, 2, c);
        fill_rect(x+4, y+1, 1, 2, c); fill_rect(x+1, y+3, 3, 1, c);
        fill_rect(x,   y+4, 1, 2, c); fill_rect(x+4, y+4, 1, 2, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case '9':
        fill_rect(x+1, y,   3, 1, c); fill_rect(x,   y+1, 1, 2, c);
        fill_rect(x+4, y+1, 1, 5, c); fill_rect(x+1, y+3, 3, 1, c);
        fill_rect(x+1, y+6, 3, 1, c);
        break;
    case '-':
        fill_rect(x,   y+3, 5, 1, c);
        break;
    default: break;
    }
}

void draw_label(int x, int y, const char *s, short color)
{
    int cx = x;
    while (*s) {
        if (*s != ' ') draw_char(cx, y, *s, color);
        cx += 6;
        s++;
    }
}

void draw_string(int x, int y, const char *s, short color, short shadow)
{
    int cx = x;
    while (*s) {
        if (*s != ' ') {
            if (shadow != color) draw_char(cx+1, y+1, *s, shadow);
            draw_char(cx, y, *s, color);
        }
        cx += 6;
        s++;
    }
}

/* ════════════════════════════════════════════════════════════
 * LARGE FONT (2x scale) for screens
 * ════════════════════════════════════════════════════════════ */
void draw_char_large(int x, int y, char ch, short c)
{
    /* Draw at 2x scale by calling draw_char on a 2x grid */
    /* We fake 2x by drawing each pixel as a 2x2 block.
     * Simplest approach: call fill_rect for each segment at 2x size. */
    int sx = x, sy = y;
    /* We'll just use draw_label scaled — draw the char twice offset */
    /* Actually implement proper 2x: reuse draw_char logic at scale 2 */
    switch(ch) {
    case 'P':
        fill_rect(sx,    sy,    4, 14, c);
        fill_rect(sx+4,  sy,    6,  2, c); fill_rect(sx+4,  sy+6, 6, 2, c);
        fill_rect(sx+10, sy+2,  2,  4, c);
        break;
    case 'E':
        fill_rect(sx,    sy,    4, 14, c);
        fill_rect(sx+4,  sy,    6,  2, c); fill_rect(sx+4,  sy+6, 4, 2, c);
        fill_rect(sx+4,  sy+12, 6,  2, c);
        break;
    case 'T':
        fill_rect(sx,    sy,    14, 2, c);
        fill_rect(sx+6,  sy+2,   2, 12, c);
        break;
    case 'C':
        fill_rect(sx+2,  sy,    10, 2, c); fill_rect(sx+2,  sy+12, 10, 2, c);
        fill_rect(sx,    sy+2,   2, 10, c);
        break;
    case 'A':
        fill_rect(sx+2,  sy,    10, 2, c);
        fill_rect(sx,    sy+2,   2, 12, c); fill_rect(sx+12, sy+2,   2, 12, c);
        fill_rect(sx+2,  sy+6,  10,  2, c);
        break;
    case 'F':
        fill_rect(sx,    sy,    4, 14, c);
        fill_rect(sx+4,  sy,    6,  2, c); fill_rect(sx+4,  sy+6,  4,  2, c);
        break;
    case 'G':
        fill_rect(sx+2,  sy,    10, 2, c); fill_rect(sx+2,  sy+12, 10, 2, c);
        fill_rect(sx,    sy+2,   2, 10, c);
        fill_rect(sx+8,  sy+6,   4,  2, c); fill_rect(sx+10, sy+8,   2,  4, c);
        break;
    case 'M':
        fill_rect(sx,    sy,    2, 14, c); fill_rect(sx+12, sy,    2, 14, c);
        fill_rect(sx+2,  sy+2,  2,  2, c); fill_rect(sx+10, sy+2,  2,  2, c);
        fill_rect(sx+4,  sy+4,  2,  2, c); fill_rect(sx+8,  sy+4,  2,  2, c);
        fill_rect(sx+6,  sy+4,  2,  4, c);
        break;
    case 'D':
        fill_rect(sx,    sy,    2, 14, c);
        fill_rect(sx+2,  sy,    6,  2, c); fill_rect(sx+2,  sy+12, 6,  2, c);
        fill_rect(sx+8,  sy+2,  2, 10, c);
        break;
    case 'O':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx+2,  sy+12, 8,  2, c);
        fill_rect(sx,    sy+2,  2, 10, c); fill_rect(sx+10, sy+2,  2, 10, c);
        break;
    case 'B':
        fill_rect(sx,    sy,    2, 14, c);
        fill_rect(sx+2,  sy,    6,  2, c); fill_rect(sx+2,  sy+6,  6,  2, c);
        fill_rect(sx+2,  sy+12, 6,  2, c);
        fill_rect(sx+8,  sy+2,  2,  4, c); fill_rect(sx+8,  sy+8,  2,  4, c);
        break;
    case 'I':
        fill_rect(sx,    sy,    10, 2, c);
        fill_rect(sx,    sy+12, 10, 2, c);
        fill_rect(sx+4,  sy+2,   2, 10, c);
        break;
    case 'R':
        fill_rect(sx,    sy,    2, 14, c);
        fill_rect(sx+2,  sy,    6,  2, c); fill_rect(sx+2,  sy+6,  6,  2, c);
        fill_rect(sx+8,  sy+2,  2,  4, c);
        fill_rect(sx+4,  sy+8,  2,  2, c); fill_rect(sx+6,  sy+10, 2,  4, c);
        break;
    case 'H':
        fill_rect(sx,    sy,    2, 14, c); fill_rect(sx+10, sy,    2, 14, c);
        fill_rect(sx+2,  sy+6,  8,  2, c);
        break;
    case 'S':
        fill_rect(sx+2,  sy,    10, 2, c);
        fill_rect(sx,    sy+2,   2,  4, c);
        fill_rect(sx+2,  sy+6,   8,  2, c);
        fill_rect(sx+10, sy+8,   2,  4, c);
        fill_rect(sx,    sy+12, 10,  2, c); fill_rect(sx+10, sy+12, 2, 2, c);
        break;
    case 'N':
        fill_rect(sx,    sy,    2, 14, c); fill_rect(sx+10, sy,    2, 14, c);
        fill_rect(sx+2,  sy+2,  2,  2, c);
        fill_rect(sx+4,  sy+4,  2,  4, c);
        fill_rect(sx+8,  sy+8,  2,  2, c);
        break;
    case 'Y':
        fill_rect(sx,    sy,    2,  6, c); fill_rect(sx+10, sy,    2,  6, c);
        fill_rect(sx+2,  sy+4,  2,  2, c); fill_rect(sx+8,  sy+4,  2,  2, c);
        fill_rect(sx+4,  sy+6,  4,  8, c);
        break;
    case 'U':
        fill_rect(sx,    sy,    2, 12, c); fill_rect(sx+10, sy,    2, 12, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case 'L':
        fill_rect(sx,    sy,    2, 14, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case 'W':
        fill_rect(sx,    sy,    2, 14, c); fill_rect(sx+12, sy,    2, 14, c);
        fill_rect(sx+2,  sy+10, 2,  2, c); fill_rect(sx+10, sy+10, 2,  2, c);
        fill_rect(sx+4,  sy+10, 4,  4, c);
        break;
    case '0':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx+2,  sy+12, 8,  2, c);
        fill_rect(sx,    sy+2,  2, 10, c); fill_rect(sx+10, sy+2,  2, 10, c);
        break;
    case '1':
        fill_rect(sx+2,  sy,    4,  2, c);
        fill_rect(sx+4,  sy,    2, 14, c);
        fill_rect(sx+2,  sy+12, 6,  2, c);
        break;
    case '2':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx+10, sy+2,  2,  4, c);
        fill_rect(sx+2,  sy+6,  8,  2, c); fill_rect(sx,    sy+8,  2,  4, c);
        fill_rect(sx+2,  sy+12, 10, 2, c);
        break;
    case '3':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx+10, sy+2,  2,  4, c);
        fill_rect(sx+2,  sy+6,  8,  2, c); fill_rect(sx+10, sy+8,  2,  4, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case '4':
        fill_rect(sx,    sy,    2,  8, c); fill_rect(sx+10, sy,    2, 14, c);
        fill_rect(sx+2,  sy+6,  8,  2, c);
        break;
    case '5':
        fill_rect(sx,    sy,    12, 2, c); fill_rect(sx,    sy+2,  2,  4, c);
        fill_rect(sx+2,  sy+6,  8,  2, c); fill_rect(sx+10, sy+8,  2,  4, c);
        fill_rect(sx,    sy+12, 10, 2, c);
        break;
    case '6':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx,    sy+2,  2, 10, c);
        fill_rect(sx+2,  sy+6,  8,  2, c); fill_rect(sx+10, sy+8,  2,  4, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case '7':
        fill_rect(sx,    sy,    12, 2, c); fill_rect(sx+10, sy+2,  2,  4, c);
        fill_rect(sx+8,  sy+6,  2,  4, c); fill_rect(sx+6,  sy+10, 2,  4, c);
        break;
    case '8':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx,    sy+2,  2,  4, c);
        fill_rect(sx+10, sy+2,  2,  4, c); fill_rect(sx+2,  sy+6,  8,  2, c);
        fill_rect(sx,    sy+8,  2,  4, c); fill_rect(sx+10, sy+8,  2,  4, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case '9':
        fill_rect(sx+2,  sy,    8,  2, c); fill_rect(sx,    sy+2,  2,  4, c);
        fill_rect(sx+10, sy+2,  2, 10, c); fill_rect(sx+2,  sy+6,  8,  2, c);
        fill_rect(sx+2,  sy+12, 8,  2, c);
        break;
    case '-':
        fill_rect(sx,    sy+6,  14, 2, c);
        break;
    case '!':
        fill_rect(sx+4,  sy,    4, 10, c);
        fill_rect(sx+4,  sy+12, 4,  2, c);
        break;
    default: break;
    }
}

void draw_label_large(int x, int y, const char *s, short color)
{
    int cx = x;
    while (*s) {
        if (*s != ' ') draw_char_large(cx, y, *s, color);
        cx += 14;
        s++;
    }
}

/* ════════════════════════════════════════════════════════════
 * HUD SCORE DISPLAY
 * ════════════════════════════════════════════════════════════ */
#define SCORE_HUD_X  132
#define SCORE_HUD_Y    2
#define SCORE_HUD_W   24

void draw_score(int score)
{
    fill_rect(SCORE_HUD_X, SCORE_HUD_Y, SCORE_HUD_W, 8, HUD_BG);
    char buf[5];
    int i = 0;
    int val = score;
    if (val < 0) { buf[i++] = '-'; val = -val; }
    if (val >= 100) {
        buf[i++] = '0' + (val / 100); val %= 100;
        buf[i++] = '0' + (val / 10);  val %= 10;
        buf[i++] = '0' + val;
    } else if (val >= 10) {
        buf[i++] = '0' + (val / 10); val %= 10;
        buf[i++] = '0' + val;
    } else {
        buf[i++] = '0' + val;
    }
    buf[i] = '\0';
    draw_label(SCORE_HUD_X, SCORE_HUD_Y, buf, WHITE);
}

/* ════════════════════════════════════════════════════════════
 * BACKGROUND
 * ════════════════════════════════════════════════════════════ */
void draw_background(void)
{
    fill_rect(0, 0, 320, 240, SKY_TAN);
    fill_rect(0,   12, 18, 188, WALL_GREEN);
    fill_rect(302, 12, 18, 188, WALL_GREEN);
    int y;
    for (y = 107; y < 190; y++) {
        short col = ((y-107) % 8 < 2) ? FLOOR_DARK : FLOOR_LIGHT;
        fill_rect(18, y, 284, 1, col);
    }
    fill_rect(0, 190, 320, 50, FLOOR_FOOD);
    fill_rect(0, 190, 320, 2, FENCE_DARK);
}

/* ════════════════════════════════════════════════════════════
 * HUD BAR
 * ════════════════════════════════════════════════════════════ */
void draw_hud(void)
{
    fill_rect(0, 0, 320, 12, HUD_BG);
    int i;
    for (i = 0; i < 320; i++) plot_pixel(i, 12, WHITE);

    draw_label(4,   2, "TIME",  WHITE);
    draw_label(28,  2, "45S",   WHITE);
    draw_label(100, 2, "SCORE", WHITE);
    draw_label(214, 2, "HI",    WHITE);
    draw_label(229, 2, "SCORE", WHITE);
    draw_label(261, 2, "012",   WHITE);
}

/* ════════════════════════════════════════════════════════════
 * STALLS
 * ════════════════════════════════════════════════════════════ */
void draw_stall(int x, short bg)
{
    fill_rect(x+POST_W, STALL_TOP, STALL_W-POST_W, STALL_BOT-STALL_TOP, bg);
    fill_rect(x+POST_W, STALL_BOT-6, STALL_W-POST_W, 6, STALL_SHELF);
    draw_line(x+POST_W, STALL_BOT-7, x+STALL_W, STALL_BOT-7, FENCE_DARK);
}

void draw_fence_post(int x)
{
    fill_rect(x, STALL_TOP-4, POST_W, STALL_BOT-STALL_TOP+4, FENCE_BROWN);
    fill_rect(x, STALL_TOP-4, 1, STALL_BOT-STALL_TOP+4, FENCE_DARK);
    fill_rect(x-1, STALL_TOP-7, POST_W+2, 4, FENCE_BROWN);
    draw_rect_border(x-1, STALL_TOP-7, POST_W+2, 4, FENCE_DARK);
    fill_rect(x+POST_W, STALL_TOP+10, 52, 3, FENCE_BROWN);
    fill_rect(x+POST_W, STALL_TOP+25, 52, 3, FENCE_BROWN);
    draw_line(x+POST_W, STALL_TOP+10, x+POST_W+52, STALL_TOP+10, FENCE_DARK);
    draw_line(x+POST_W, STALL_TOP+25, x+POST_W+52, STALL_TOP+25, FENCE_DARK);
}

void draw_all_stalls(void)
{
    int xs[5]   = {18, 74, 130, 186, 242};
    short cs[5] = {STALL_BLUE, STALL_BLUE, STALL_BLUE, STALL_BLUE, STALL_BLUE};
    int pxs[6]  = {18, 74, 130, 186, 242, 298};
    int i;
    for (i = 0; i < 5; i++) draw_stall(xs[i], cs[i]);
    for (i = 0; i < 6; i++) draw_fence_post(pxs[i]);
}

/* Draw or clear the highlight for one stall.
 * Strategy: redraw the stall with the chosen bg colour, then
 * repaint the two fence posts that border it so no yellow
 * residue is left when turning the highlight off.            */
void draw_stall_highlight(int animal_id, int on)
{
    int stall_left[5] = {18, 74, 130, 186, 242};
    /* Each stall is bordered by fence post at stall_left[i]
     * on the left and stall_left[i]+56 on the right (== stall_left[i+1]). */
    int post_left[6]  = {18, 74, 130, 186, 242, 298};

    int x  = stall_left[animal_id];
    short bg = on ? STALL_HIGHLIGHT : STALL_BLUE;

    /* 1. Repaint the stall interior */
    draw_stall(x, bg);

    /* 2. Repaint the two fence posts that frame this stall so any
     *    highlight pixels underneath them are covered. */
    draw_fence_post(post_left[animal_id]);
    draw_fence_post(post_left[animal_id + 1]);
}

/* ════════════════════════════════════════════════════════════
 * TITLE BANNER
 * ════════════════════════════════════════════════════════════ */
void draw_title_banner(void)
{
    fill_rect(132, 16, 55, 22, BANNER_BG);
    draw_rect_border(132, 16, 55, 22, BANNER_BDR);
    draw_rect_border(133, 17, 53, 20, BANNER_BDR);
    draw_string(136, 19, "PET CAFE", TITLE_COL, TITLE_COL);
    draw_string(148, 28, "GAME",     TITLE_COL, TITLE_COL);
}

/* ════════════════════════════════════════════════════════════
 * ANIMAL SPRITES
 * ════════════════════════════════════════════════════════════ */
void draw_cat(int cx, int cy)
{
    int i;
    for (i = 0; i < 5; i++) fill_circle(cx+14+i*3, cy+8-i, 2, 2, C_DARK_ORG);
    fill_circle(cx, cy+8, 12, 10, C_ORANGE);
    fill_circle(cx, cy+10, 6, 7, C_LIGHT_ORG);
    draw_line(cx-8, cy+2,  cx-4, cy+6,  C_DARK_ORG);
    draw_line(cx-7, cy+5,  cx-3, cy+9,  C_DARK_ORG);
    draw_line(cx+4, cy+2,  cx+8, cy+6,  C_DARK_ORG);
    draw_line(cx+3, cy+5,  cx+7, cy+9,  C_DARK_ORG);
    fill_circle(cx-5, cy+17, 4, 3, C_ORANGE); fill_circle(cx+5, cy+17, 4, 3, C_ORANGE);
    fill_circle(cx-5, cy+19, 3, 2, C_LIGHT_ORG); fill_circle(cx+5, cy+19, 3, 2, C_LIGHT_ORG);
    fill_circle(cx, cy-2, 6, 5, C_ORANGE);
    fill_circle(cx, cy-12, 10, 9, C_ORANGE);
    fill_triangle(cx-10,cy-15, cx-6,cy-22, cx-2,cy-14, C_DARK_ORG);
    fill_triangle(cx+10,cy-15, cx+6,cy-22, cx+2,cy-14, C_DARK_ORG);
    fill_triangle(cx-9, cy-16, cx-6,cy-20, cx-3,cy-15, C_PINK);
    fill_triangle(cx+9, cy-16, cx+6,cy-20, cx+3,cy-15, C_PINK);
    draw_line(cx-2, cy-19, cx-1, cy-15, C_DARK_ORG);
    draw_line(cx,   cy-20, cx,   cy-15, C_DARK_ORG);
    draw_line(cx+2, cy-19, cx+1, cy-15, C_DARK_ORG);
    fill_circle(cx-4, cy-12, 3, 3, WHITE); fill_circle(cx+4, cy-12, 3, 3, WHITE);
    fill_circle(cx-4, cy-12, 2, 2, C_GREEN); fill_circle(cx+4, cy-12, 2, 2, C_GREEN);
    fill_circle(cx-4, cy-12, 1, 2, BLACK); fill_circle(cx+4, cy-12, 1, 2, BLACK);
    plot_pixel(cx-2, cy-14, WHITE); plot_pixel(cx-1, cy-14, WHITE);
    plot_pixel(cx+5, cy-14, WHITE); plot_pixel(cx+6, cy-14, WHITE);
    fill_triangle(cx-2,cy-7, cx+2,cy-7, cx,cy-5, C_NOSE);
    draw_line(cx, cy-5, cx-2, cy-3, BLACK); draw_line(cx, cy-5, cx+2, cy-3, BLACK);
    draw_line(cx-4, cy-6, cx-14,cy-7, WHITE); draw_line(cx-4, cy-5, cx-14,cy-5, WHITE);
    draw_line(cx+4, cy-6, cx+14,cy-7, WHITE); draw_line(cx+4, cy-5, cx+14,cy-5, WHITE);
}

void draw_dog(int cx, int cy)
{
    int i;
    for (i = 0; i < 4; i++) fill_circle(cx+13+i*3, cy+5-i*2, 2, 2, D_GOLD);
    fill_circle(cx, cy+8, 12, 10, D_GOLD);
    fill_circle(cx, cy+11, 6, 7, 0xFEE8);
    fill_circle(cx-5, cy+17, 3, 3, D_GOLD); fill_circle(cx+5, cy+17, 3, 3, D_GOLD);
    fill_circle(cx-5, cy+19, 3, 2, D_DARK); fill_circle(cx+5, cy+19, 3, 2, D_DARK);
    fill_circle(cx, cy-1, 7, 6, D_GOLD);
    fill_circle(cx, cy-12, 11, 10, D_GOLD);
    fill_circle(cx-11, cy-8, 3, 8, D_DARK); fill_circle(cx+11, cy-8, 3, 8, D_DARK);
    fill_circle(cx, cy-7, 6, 4, D_DARK);
    fill_circle(cx, cy-6, 4, 3, 0xFEE8);
    fill_circle(cx-4, cy-14, 3, 3, WHITE); fill_circle(cx+4, cy-14, 3, 3, WHITE);
    fill_circle(cx-4, cy-14, 2, 2, D_IRIS); fill_circle(cx+4, cy-14, 2, 2, D_IRIS);
    fill_circle(cx-4, cy-14, 1, 2, BLACK); fill_circle(cx+4, cy-14, 1, 2, BLACK);
    plot_pixel(cx-2, cy-16, WHITE); plot_pixel(cx-1, cy-16, WHITE);
    plot_pixel(cx+5, cy-16, WHITE); plot_pixel(cx+6, cy-16, WHITE);
    fill_circle(cx, cy-7, 3, 2, D_NOSE);
    fill_circle(cx, cy-3, 3, 2, D_TONGUE); fill_circle(cx, cy-1, 3, 2, D_TONGUE);
    draw_line(cx, cy-3, cx, cy-1, 0xCBB5);
}

void draw_bird(int cx, int cy)
{
    fill_triangle(cx-3, cy+16, cx+3, cy+16, cx, cy+24, B_DARK);
    fill_triangle(cx-2, cy+16, cx+2, cy+16, cx, cy+22, B_BODY);
    fill_circle(cx, cy+6, 10, 11, B_BODY);
    fill_circle(cx-8, cy+6, 4, 8, B_WING);
    fill_circle(cx, cy-6, 10, 9, B_HEAD);
    draw_line(cx-3, cy-13, cx-1, cy-9, B_BODY);
    draw_line(cx,   cy-14, cx,   cy-10, B_BODY);
    draw_line(cx+3, cy-13, cx+1, cy-9, B_BODY);
    fill_circle(cx-7, cy-4, 3, 2, B_CHEEK); fill_circle(cx+7, cy-4, 3, 2, B_CHEEK);
    fill_triangle(cx-2,cy-7, cx+2,cy-7, cx,cy-3, B_BEAK);
    fill_circle(cx, cy-8, 2, 1, 0xCDF5);
    fill_circle(cx-3, cy-10, 3, 3, WHITE); fill_circle(cx+3, cy-10, 3, 3, WHITE);
    fill_circle(cx-3, cy-10, 2, 2, B_BODY); fill_circle(cx+3, cy-10, 2, 2, B_BODY);
    fill_circle(cx-3, cy-10, 1, 2, BLACK); fill_circle(cx+3, cy-10, 1, 2, BLACK);
    plot_pixel(cx-1, cy-12, WHITE); plot_pixel(cx-2, cy-12, WHITE);
    plot_pixel(cx+4, cy-12, WHITE); plot_pixel(cx+5, cy-12, WHITE);
    draw_line(cx-2, cy+16, cx-4, cy+21, B_BEAK); draw_line(cx+2, cy+16, cx+4, cy+21, B_BEAK);
    draw_line(cx-4, cy+21, cx-7, cy+21, B_BEAK); draw_line(cx+4, cy+21, cx+7, cy+21, B_BEAK);
}

void draw_hamster(int cx, int cy)
{
    fill_circle(cx, cy+8, 11, 10, H_BODY);
    fill_circle(cx, cy+11, 6, 7, WHITE);
    fill_circle(cx-10, cy+6, 3, 2, H_DARK); fill_circle(cx+10, cy+6, 3, 2, H_DARK);
    fill_circle(cx-5, cy+17, 4, 2, H_DARK); fill_circle(cx+5, cy+17, 4, 2, H_DARK);
    fill_circle(cx, cy-5, 11, 10, H_BODY);
    fill_circle(cx-11, cy-2, 6, 5, H_DARK); fill_circle(cx+11, cy-2, 6, 5, H_DARK);
    fill_circle(cx-7, cy-13, 5, 4, H_PINK); fill_circle(cx+7, cy-13, 5, 4, H_PINK);
    fill_circle(cx-7, cy-13, 3, 2, H_DARK); fill_circle(cx+7, cy-13, 3, 2, H_DARK);
    fill_circle(cx-4, cy-7, 4, 4, WHITE); fill_circle(cx+4, cy-7, 4, 4, WHITE);
    fill_circle(cx-4, cy-7, 3, 3, H_IRIS); fill_circle(cx+4, cy-7, 3, 3, H_IRIS);
    fill_circle(cx-4, cy-7, 1, 2, BLACK); fill_circle(cx+4, cy-7, 1, 2, BLACK);
    plot_pixel(cx-2, cy-9, WHITE); plot_pixel(cx-1, cy-9, WHITE);
    plot_pixel(cx+5, cy-9, WHITE); plot_pixel(cx+6, cy-9, WHITE);
    fill_circle(cx, cy-2, 2, 1, H_PINK);
    draw_line(cx, cy-1, cx-2, cy+1, BLACK); draw_line(cx, cy-1, cx+2, cy+1, BLACK);
}

void draw_rabbit(int cx, int cy)
{
    fill_circle(cx-10, cy+10, 4, 4, WHITE);
    fill_circle(cx, cy+8, 11, 11, R_WHITE);
    fill_circle(cx, cy+11, 6, 7, 0xFEFF);
    fill_circle(cx-6, cy+18, 6, 3, R_WHITE); fill_circle(cx+6, cy+18, 6, 3, R_WHITE);
    fill_circle(cx, cy-1, 6, 5, R_WHITE);
    fill_circle(cx, cy-11, 10, 9, R_WHITE);
    fill_circle(cx-4, cy-23, 4, 11, R_WHITE); fill_circle(cx+4, cy-23, 4, 11, R_WHITE);
    fill_circle(cx-4, cy-23, 2, 9, R_PINK);   fill_circle(cx+4, cy-23, 2, 9, R_PINK);
    fill_circle(cx-8, cy-9, 4, 3, WHITE); fill_circle(cx+8, cy-9, 4, 3, WHITE);
    fill_circle(cx-4, cy-13, 3, 3, WHITE); fill_circle(cx+4, cy-13, 3, 3, WHITE);
    fill_circle(cx-4, cy-13, 2, 2, R_EYE); fill_circle(cx+4, cy-13, 2, 2, R_EYE);
    fill_circle(cx-4, cy-13, 1, 2, BLACK); fill_circle(cx+4, cy-13, 1, 2, BLACK);
    plot_pixel(cx-2, cy-15, WHITE); plot_pixel(cx-1, cy-15, WHITE);
    plot_pixel(cx+5, cy-15, WHITE); plot_pixel(cx+6, cy-15, WHITE);
    fill_circle(cx, cy-7, 2, 1, R_PINK);
    draw_line(cx, cy-6, cx-2, cy-4, BLACK); draw_line(cx, cy-6, cx+2, cy-4, BLACK);
    draw_line(cx-2, cy-6, cx-11, cy-7, LIGHT_GREY); draw_line(cx+2, cy-6, cx+11, cy-7, LIGHT_GREY);
}

/* Draw the correct animal based on ID */
void draw_animal(int id, int cx, int cy)
{
    if      (id == 0) draw_cat    (cx, cy);
    else if (id == 1) draw_dog    (cx, cy);
    else if (id == 2) draw_bird   (cx, cy);
    else if (id == 3) draw_hamster(cx, cy);
    else              draw_rabbit (cx, cy);
}

/* ════════════════════════════════════════════════════════════
 * HUMAN PLAYER
 * ════════════════════════════════════════════════════════════ */
void draw_human(int cx, int cy)
{
    fill_circle(cx-4,  cy+21, 5, 2, P_SHOE); fill_circle(cx+4, cy+21, 5, 2, P_SHOE);
    fill_rect(cx-6, cy+10, 4, 12, P_PANTS); fill_rect(cx+2, cy+10, 4, 12, P_PANTS);
    fill_rect(cx-7, cy-2, 14, 13, P_SHIRT);
    fill_rect(cx-2, cy-2,  4,  3, P_SKIN);
    fill_rect(cx-10, cy+1, 3,  9, P_SHIRT);
    fill_circle(cx-8, cy+11, 3, 3, P_SKIN);
    fill_rect(cx+7,  cy-2, 3,  9, P_SHIRT);
    fill_circle(cx+9, cy+8,  3, 3, P_SKIN);
    fill_circle(cx, cy-11, 9, 9, P_SKIN);
    fill_circle(cx,   cy-18, 9, 4, P_HAIR);
    fill_circle(cx-7, cy-14, 3, 5, P_HAIR); fill_circle(cx+7, cy-14, 3, 5, P_HAIR);
    fill_circle(cx-3, cy-12, 2, 2, WHITE); fill_circle(cx+3, cy-12, 2, 2, WHITE);
    fill_circle(cx-3, cy-12, 1, 2, BLACK); fill_circle(cx+3, cy-12, 1, 2, BLACK);
    plot_pixel(cx-2, cy-14, WHITE); plot_pixel(cx+4, cy-14, WHITE);
    draw_line(cx-2, cy-6, cx-1, cy-5, BLACK);
    draw_line(cx-1, cy-5, cx+1, cy-5, BLACK);
    draw_line(cx+1, cy-5, cx+2, cy-6, BLACK);
}

/* ════════════════════════════════════════════════════════════
 * SPRITE SAVE / RESTORE
 * ════════════════════════════════════════════════════════════ */
#define SPRITE_W  31
#define SPRITE_H  48
#define SPRITE_X0 (-15)
#define SPRITE_Y0 (-23)

static short bg_patch[SPRITE_H][SPRITE_W];

void save_bg(int cx, int cy)
{
    int px, py, bx, by;
    for (by = 0; by < SPRITE_H; by++) {
        py = cy + SPRITE_Y0 + by;
        for (bx = 0; bx < SPRITE_W; bx++) {
            px = cx + SPRITE_X0 + bx;
            if (px >= 0 && px < 320 && py >= 0 && py < 240)
                bg_patch[by][bx] = pixel_buf[py * 512 + px];
            else
                bg_patch[by][bx] = 0;
        }
    }
}

void restore_bg(int cx, int cy)
{
    int px, py, bx, by;
    for (by = 0; by < SPRITE_H; by++) {
        py = cy + SPRITE_Y0 + by;
        for (bx = 0; bx < SPRITE_W; bx++) {
            px = cx + SPRITE_X0 + bx;
            if (px >= 0 && px < 320 && py >= 0 && py < 240)
                pixel_buf[py * 512 + px] = bg_patch[by][bx];
        }
    }
}

/* ════════════════════════════════════════════════════════════
 * FOOD ITEMS & BOXES
 * ════════════════════════════════════════════════════════════ */
void draw_food_box(int cx, int cy, int selected)
{
    short bg  = selected ? BOX_SEL : BOX_GREY;
    short hi  = selected ? 0x67E0  : WHITE;
    fill_rect(cx-21, cy-18, 42, 36, bg);
    draw_rect_border(cx-21, cy-18, 42, 36, BLACK);
    draw_rect_border(cx-20, cy-17, 40, 34, hi);
}

void draw_food_fish(int cx, int cy)
{
    int fx = cx - 4;
    fill_triangle(fx+11,cy, fx+18,cy-6, fx+18,cy+6, F_FISH_B);
    fill_circle(fx, cy, 11, 7, F_FISH_B);
    fill_circle(fx-2, cy+1, 7, 5, F_FISH_L);
    fill_triangle(fx-1,cy-7, fx+5,cy-7, fx+2,cy-12, F_FISH_B);
    fill_circle(fx-6, cy-2, 3, 3, WHITE);
    fill_circle(fx-6, cy-2, 1, 2, BLACK);
    plot_pixel(fx-5, cy-3, WHITE);
}

void draw_food_bone(int cx, int cy)
{
    fill_circle(cx-10, cy-3, 6, 5, MID_GREY); fill_circle(cx-10, cy+3, 6, 5, MID_GREY);
    fill_circle(cx+10, cy-3, 6, 5, MID_GREY); fill_circle(cx+10, cy+3, 6, 5, MID_GREY);
    fill_rect(cx-10, cy-4, 20, 8, MID_GREY);
    fill_circle(cx-10, cy-3, 5, 4, F_BONE); fill_circle(cx-10, cy+3, 5, 4, F_BONE);
    fill_circle(cx+10, cy-3, 5, 4, F_BONE); fill_circle(cx+10, cy+3, 5, 4, F_BONE);
    fill_rect(cx-9, cy-2, 18, 5, F_BONE);
}

void draw_food_seeds(int cx, int cy)
{
    fill_circle(cx, cy+6, 12, 4, MID_GREY);
    int sx[10] = {-6,-2, 2, 6,-5, 1, 4,-1, 3,-4};
    int sy[10] = { 2, 1, 3, 2, 5, 5, 6, 7, 0,-1};
    int s;
    for (s = 0; s < 10; s++) {
        fill_circle(cx+sx[s], cy+sy[s], 2, 2, F_SEED_L);
        plot_pixel(cx+sx[s], cy+sy[s], F_SEED_D);
    }
}

void draw_food_carrot(int cx, int cy)
{
    fill_triangle(cx-7,cy-10, cx+7,cy-10, cx,cy+10, F_CARROT);
    draw_line(cx-5, cy-5, cx+5, cy-5, 0xE480);
    draw_line(cx-4, cy,   cx+4, cy,   0xE480);
    draw_line(cx-2, cy+5, cx+2, cy+5, 0xE480);
    fill_triangle(cx,   cy-10, cx-6,cy-19, cx-1,cy-12, F_LEAF);
    fill_triangle(cx,   cy-10, cx+6,cy-19, cx+1,cy-12, F_LEAF);
    fill_triangle(cx-1, cy-10, cx-2,cy-16, cx+3,cy-13, F_LEAF);
}

/* Redraw a food box + its icon */
void redraw_food_slot(int slot, int carried_food)
{
    int cx;
    if      (slot == 0) cx = F0_X;
    else if (slot == 1) cx = F1_X;
    else if (slot == 2) cx = F2_X;
    else                cx = F3_X;

    int selected = (carried_food == slot);
    draw_food_box(cx, FOOD_Y, selected);
    if      (slot == 0) draw_food_fish  (cx, FOOD_Y);
    else if (slot == 1) draw_food_bone  (cx, FOOD_Y);
    else if (slot == 2) draw_food_seeds (cx, FOOD_Y);
    else                draw_food_carrot(cx, FOOD_Y);
}

/* ════════════════════════════════════════════════════════════
 * FOOD COLLISION (pick-up zone)
 * ════════════════════════════════════════════════════════════ */
int check_food_collision(int px, int py)
{
    if (py < 168) return -1;
    int food_xs[4] = {F0_X, F1_X, F2_X, F3_X};
    int i;
    for (i = 0; i < 4; i++) {
        int dx = px - food_xs[i];
        if (dx < 0) dx = -dx;
        if (dx <= 21) return i;
    }
    return -1;
}

/* Drop-off: player is at the top of the food area (standing at fence)
 * and x-aligned with the requested animal's column.                 */
int check_dropoff(int px, int py, int animal_id)
{
    if (animal_id < 0) return 0;
    if (py > DROPOFF_Y_MAX) return 0;
    int dx = px - animal_cx[animal_id];
    if (dx < 0) dx = -dx;
    return (dx <= 26);
}

/* ════════════════════════════════════════════════════════════
 * PS/2 KEYBOARD
 * ════════════════════════════════════════════════════════════ */
int ps2_read_byte(void)
{
    int data = *ps2_ptr;
    if (data & 0x8000) return data & 0xFF;
    return -1;
}

/* Flush any pending PS/2 bytes (used between screens) */
void ps2_flush(void)
{
    int timeout = 10000;
    while (timeout-- > 0) {
        int d = *ps2_ptr;
        if (!(d & 0x8000)) break;
    }
}

/* Block until any key is pressed (make code received) */
void wait_any_key(void)
{
    int ext = 0, brk = 0;
    while (1) {
        int b = ps2_read_byte();
        if (b < 0) continue;
        if (b == PS2_EXT)   { ext = 1; continue; }
        if (b == PS2_BREAK) { brk = 1; continue; }
        if (brk) { brk = 0; ext = 0; continue; }   /* ignore break codes */
        /* Got a make code — any key is fine */
        ext = 0;
        return;
    }
}

/* ════════════════════════════════════════════════════════════
 * START SCREEN
 * ════════════════════════════════════════════════════════════ */
void draw_start_screen(void)
{
    /* Dark background */
    fill_rect(0, 0, 320, 240, 0x0821);   /* very dark teal-black */

    /* Decorative border */
    draw_rect_border(4,  4,  312, 232, 0xFFE0);
    draw_rect_border(8,  8,  304, 224, 0xFD60);
    draw_rect_border(12, 12, 296, 216, 0xFFE0);

    /* Title: PET CAFE */
    draw_label_large(44,  30, "PET",  0xFFE0);
    draw_label_large(44,  50, "CAFE", 0xFD60);
    draw_label_large(44,  70, "GAME", WHITE);

    /* Decorative animals summary line */
    /* Draw mini versions of the animals as teasers */
    draw_cat    (50,  135);
    draw_dog    (100, 135);
    draw_bird   (150, 135);
    draw_hamster(200, 135);
    draw_rabbit (250, 135);

    /* Instructions */
    draw_label(55,  165, "FEED THE RIGHT ANIMAL", 0xFFE0);
    draw_label(70,  175, "WITH THE RIGHT FOOD",   0xFD60);

    /* Controls hint */
    draw_label(60, 190, "USE ARROW KEYS TO MOVE", LIGHT_GREY);

    /* Press any key prompt */
    draw_label(72, 210, "PRESS ANY KEY TO START", 0x07FF);

    /* Blinking dots decoration */
    fill_circle(30,  215, 3, 3, 0xFFE0);
    fill_circle(290, 215, 3, 3, 0xFFE0);
}

/* ════════════════════════════════════════════════════════════
 * END SCREEN
 * ════════════════════════════════════════════════════════════ */
void draw_end_screen(int score)
{
    fill_rect(0, 0, 320, 240, 0x0821);

    draw_rect_border(4,  4,  312, 232, 0xFFE0);
    draw_rect_border(8,  8,  304, 224, 0xFD60);
    draw_rect_border(12, 12, 296, 216, 0xFFE0);

    /* GAME OVER title */
    draw_label_large(30,  30, "GAME", 0xF800);
    draw_label_large(30,  50, "OVER", 0xF800);
    draw_label_large(30,  70, "!",    0xFFE0);

    /* Score display */
    draw_label(70, 105, "YOUR SCORE", WHITE);
    draw_label(70, 118, "WAS", WHITE);

    /* Build score string */
    char sbuf[8];
    int idx = 0;
    int val = score;
    if (val < 0) { sbuf[idx++] = '-'; val = -val; }
    if (val >= 100) {
        sbuf[idx++] = '0' + (val / 100); val %= 100;
        sbuf[idx++] = '0' + (val / 10);  val %= 10;
        sbuf[idx++] = '0' + val;
    } else if (val >= 10) {
        sbuf[idx++] = '0' + (val / 10); val %= 10;
        sbuf[idx++] = '0' + val;
    } else {
        sbuf[idx++] = '0' + val;
    }
    sbuf[idx] = '\0';

    /* Draw score large */
    draw_label_large(120, 112, sbuf, 0xFFE0);

    /* Rating message */
    if (score >= 10) {
        draw_label(75, 155, "AMAZING WORK", 0x07E0);
    } else if (score >= 5) {
        draw_label(72, 155, "GOOD JOB KEEP", 0xFFE0);
        draw_label(72, 165, "IT UP", 0xFFE0);
    } else if (score >= 1) {
        draw_label(72, 155, "NICE TRY", 0xFD60);
    } else {
        draw_label(66, 155, "BETTER LUCK NEXT", 0xF800);
        draw_label(66, 165, "TIME",              0xF800);
    }

    /* Draw some animals as decoration */
    draw_cat   (50,  115);
    draw_rabbit(270, 115);

    draw_label(66, 200, "PRESS ANY KEY TO PLAY", 0x07FF);
    draw_label(90, 210, "AGAIN",                 0x07FF);
}

/* ════════════════════════════════════════════════════════════
 * DRAW FULL GAME SCENE (static elements)
 * ════════════════════════════════════════════════════════════ */
void draw_game_scene(void)
{
    draw_background();
    draw_hud();
    draw_all_stalls();
    draw_title_banner();

    draw_cat    (P_CX_0, PET_Y);
    draw_dog    (P_CX_1, PET_Y);
    draw_bird   (P_CX_2, PET_Y);
    draw_hamster(P_CX_3, PET_Y);
    draw_rabbit (P_CX_4, PET_Y);

    draw_label(P_CX_0-8,  STALL_BOT+2, "CAT",     BLACK);
    draw_label(P_CX_1-8,  STALL_BOT+2, "DOG",     BLACK);
    draw_label(P_CX_2-10, STALL_BOT+2, "BIRD",    BLACK);
    draw_label(P_CX_3-22, STALL_BOT+2, "HAMSTER", BLACK);
    draw_label(P_CX_4-16, STALL_BOT+2, "RABBIT",  BLACK);

    draw_food_box(F0_X, FOOD_Y, 0);
    draw_food_box(F1_X, FOOD_Y, 0);
    draw_food_box(F2_X, FOOD_Y, 0);
    draw_food_box(F3_X, FOOD_Y, 0);
    draw_food_fish  (F0_X, FOOD_Y);
    draw_food_bone  (F1_X, FOOD_Y);
    draw_food_seeds (F2_X, FOOD_Y);
    draw_food_carrot(F3_X, FOOD_Y);
    draw_label(F0_X-14, FOOD_Y+21, "FISH",   WHITE);
    draw_label(F1_X-14, FOOD_Y+21, "BONE",   WHITE);
    draw_label(F2_X-14, FOOD_Y+21, "SEEDS",  WHITE);
    draw_label(F3_X-20, FOOD_Y+21, "CARROT", WHITE);
}

/* ════════════════════════════════════════════════════════════
 * PICK NEXT RANDOM ANIMAL (avoid repeating current)
 * ════════════════════════════════════════════════════════════ */
int pick_new_animal(int current)
{
    int next;
    do {
        next = rand_range(5);
    } while (next == current);
    return next;
}

/* ════════════════════════════════════════════════════════════
 * MAIN
 * ════════════════════════════════════════════════════════════ */
int main(void)
{
    *(vga_ctrl + 1) = PIXEL_BASE;

    int game_state = STATE_START;

    /* Game variables declared here so they persist across state transitions */
    int score       = 0;
    int carried     = FOOD_NONE;
    int player_x    = 160;
    int player_y    = 162;
    int active_animal = -1;        /* which animal is currently requesting food */
    int prev_food   = -1;
    int ext_pending  = 0;
    int break_pending = 0;

    while (1)
    {
        /* ══════════════════════════════════════════════════
         * START SCREEN
         * ══════════════════════════════════════════════════ */
        if (game_state == STATE_START)
        {
            draw_start_screen();
            ps2_flush();
            wait_any_key();

            /* Initialise game */
            score        = 0;
            carried      = FOOD_NONE;
            player_x     = 160;
            player_y     = 162;
            prev_food    = -1;
            ext_pending  = 0;
            break_pending = 0;

            draw_game_scene();
            draw_score(score);

            /* Pick first random animal request */
            active_animal = rand_range(5);
            draw_stall_highlight(active_animal, 1);
            /* Redraw the animal on top of the highlight */
            draw_animal(active_animal, animal_cx[active_animal], PET_Y);

            /* Save clean bg (no player) then draw player on top */
            save_bg(player_x, player_y);
            draw_human(player_x, player_y);

            game_state = STATE_PLAY;
            continue;
        }

        /* ══════════════════════════════════════════════════
         * END SCREEN
         * ══════════════════════════════════════════════════ */
        if (game_state == STATE_END)
        {
            draw_end_screen(score);
            ps2_flush();
            wait_any_key();
            game_state = STATE_START;
            continue;
        }

        /* ══════════════════════════════════════════════════
         * GAMEPLAY
         * ══════════════════════════════════════════════════ */

        /* ── Food pick-up collision ──────────────────────── */
        int food_hit = check_food_collision(player_x, player_y);
        if (food_hit != prev_food) {
            if (food_hit >= 0 && carried == FOOD_NONE) {
                /* 1. Erase player using old (clean) bg patch */
                restore_bg(player_x, player_y);
                /* 2. Update carry state & redraw the food box */
                carried = food_hit;
                redraw_food_slot(food_hit, carried);
                /* 3. Save a fresh bg patch (no player, updated food box) */
                save_bg(player_x, player_y);
                /* 4. Draw player on top */
                draw_human(player_x, player_y);
            }
            prev_food = food_hit;
        }

        /* ── Drop-off at requested animal ───────────────── */
        if (carried != FOOD_NONE && check_dropoff(player_x, player_y, active_animal))
        {
            /* Evaluate correctness */
            if (carried == correct_food[active_animal])
                score++;
            else
                score--;

            /* 1. Erase player cleanly before touching any scene pixels */
            restore_bg(player_x, player_y);

            /* 2. Un-highlight food box, redraw stall/animal */
            int old_carried = carried;
            carried = FOOD_NONE;
            redraw_food_slot(old_carried, FOOD_NONE);

            draw_stall_highlight(active_animal, 0);
            draw_animal(active_animal, animal_cx[active_animal], PET_Y);

            /* 3. Pick new animal and highlight */
            active_animal = pick_new_animal(active_animal);
            draw_stall_highlight(active_animal, 1);
            draw_animal(active_animal, animal_cx[active_animal], PET_Y);

            /* 4. Update HUD score */
            draw_score(score);

            /* 5. Save fresh bg (scene fully updated, no player) then redraw player */
            save_bg(player_x, player_y);
            draw_human(player_x, player_y);

            /* End condition */
            if (score >= 10 || score <= -5)
                game_state = STATE_END;

            continue;
        }

        /* ── PS/2 keyboard input ─────────────────────────── */
        int byte = ps2_read_byte();
        if (byte < 0) continue;

        if (byte == PS2_EXT)   { ext_pending   = 1; continue; }
        if (byte == PS2_BREAK) { break_pending = 1; continue; }

        if (break_pending) {
            break_pending = 0;
            ext_pending   = 0;
            continue;
        }

        if (ext_pending) {
            ext_pending = 0;

            int new_x = player_x;
            int new_y = player_y;

            switch (byte) {
                case SC_LEFT:  new_x -= MOVE_STEP; break;
                case SC_RIGHT: new_x += MOVE_STEP; break;
                case SC_UP:    new_y -= MOVE_STEP; break;
                case SC_DOWN:  new_y += MOVE_STEP; break;
                default: continue;
            }

            /* Clamp to yellow food area only */
            if (new_x < PLAYER_X_MIN) new_x = PLAYER_X_MIN;
            if (new_x > PLAYER_X_MAX) new_x = PLAYER_X_MAX;
            if (new_y < PLAYER_Y_MIN) new_y = PLAYER_Y_MIN;
            if (new_y > PLAYER_Y_MAX) new_y = PLAYER_Y_MAX;

            if (new_x != player_x || new_y != player_y) {
                restore_bg(player_x, player_y);
                player_x = new_x;
                player_y = new_y;
                save_bg(player_x, player_y);
                draw_human(player_x, player_y);
            }
        }
    }

    return 0;
}
