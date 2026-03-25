#define VGA_BASE 0xFF203020
#define PIXEL_BASE 0x08000000

volatile int  *vga_ctrl  = (volatile int  *) VGA_BASE;
volatile short *pixel_buf = (volatile short *) PIXEL_BASE;

/* RGB565 colors */
#define ORANGE      0xFD00   /* cat body */
#define DARK_ORANGE 0xD300   /* stripes/ears */
#define LIGHT_ORANGE 0xFD80  /* belly */
#define PINK        0xF8B2   /* inner ear */
#define GREEN       0x07E0   /* eyes */
#define BLACK       0x0000
#define WHITE       0xFFFF
#define DARK_PINK   0xF810   /* nose */
#define BG_COLOR    0x8410   /* grey background */

void plot_pixel(int x, int y, short color)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    pixel_buf[y * 512 + x] = color;
}

void draw_line(int x0, int y0, int x1, int y1, short color)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx - dy;
    while (1)
    {
        plot_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void fill_rect(int x, int y, int w, int h, short color)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            plot_pixel(i, j, color);
}

void fill_circle(int cx, int cy, int rx, int ry, short color)
{
    int x, y;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if ((x * x * ry * ry + y * y * rx * rx) <= rx * rx * ry * ry)
                plot_pixel(cx + x, cy + y, color);
}

void fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, short color)
{
    int minx = x0, maxx = x0, miny = y0, maxy = y0;
    if (x1 < minx) minx = x1; if (x1 > maxx) maxx = x1;
    if (x2 < minx) minx = x2; if (x2 > maxx) maxx = x2;
    if (y1 < miny) miny = y1; if (y1 > maxy) maxy = y1;
    if (y2 < miny) miny = y2; if (y2 > maxy) maxy = y2;
    int x, y;
    for (y = miny; y <= maxy; y++)
    for (x = minx; x <= maxx; x++)
    {
        int d0 = (x1-x0)*(y-y0) - (x-x0)*(y1-y0);
        int d1 = (x2-x1)*(y-y1) - (x-x1)*(y2-y1);
        int d2 = (x0-x2)*(y-y2) - (x-x2)*(y0-y2);
        if ((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0))
            plot_pixel(x, y, color);
    }
}

void draw_cat(int cx, int cy)
{
    /* ── background ── */
    fill_rect(0, 0, 320, 240, BG_COLOR);

    /* ── tail ── */
    int i;
    for (i = 0; i < 12; i++)
        fill_circle(cx + 55 + i*3, cy + 30 - i*2, 6, 5, DARK_ORANGE);

    /* ── body ── */
    fill_circle(cx, cy + 30, 45, 38, ORANGE);

    /* ── belly ── */
    fill_circle(cx, cy + 35, 25, 28, LIGHT_ORANGE);

    /* ── body stripes ── */
    draw_line(cx - 30, cy + 10, cx - 15, cy + 25, DARK_ORANGE);
    draw_line(cx - 28, cy + 15, cx - 13, cy + 30, DARK_ORANGE);
    draw_line(cx + 15, cy + 10, cx + 30, cy + 25, DARK_ORANGE);
    draw_line(cx + 13, cy + 15, cx + 28, cy + 30, DARK_ORANGE);

    /* ── legs ── */
    fill_circle(cx - 22, cy + 62, 14, 10, ORANGE);
    fill_circle(cx + 22, cy + 62, 14, 10, ORANGE);
    /* paws */
    fill_circle(cx - 22, cy + 68, 12, 7,  LIGHT_ORANGE);
    fill_circle(cx + 22, cy + 68, 12, 7,  LIGHT_ORANGE);

    /* ── neck ── */
    fill_circle(cx, cy - 5, 24, 18, ORANGE);

    /* ── head ── */
    fill_circle(cx, cy - 35, 38, 34, ORANGE);

    /* ── ears ── */
    fill_triangle(cx - 38, cy - 55, cx - 22, cy - 75, cx - 8,  cy - 52, DARK_ORANGE);
    fill_triangle(cx + 38, cy - 55, cx + 22, cy - 75, cx + 8,  cy - 52, DARK_ORANGE);
    /* inner ears */
    fill_triangle(cx - 33, cy - 57, cx - 22, cy - 70, cx - 12, cy - 54, PINK);
    fill_triangle(cx + 33, cy - 57, cx + 22, cy - 70, cx + 12, cy - 54, PINK);

    /* ── forehead stripes ── */
    draw_line(cx - 8,  cy - 65, cx - 4,  cy - 55, DARK_ORANGE);
    draw_line(cx,      cy - 67, cx,       cy - 56, DARK_ORANGE);
    draw_line(cx + 8,  cy - 65, cx + 4,  cy - 55, DARK_ORANGE);

    /* ── eyes ── */
    fill_circle(cx - 14, cy - 35, 10, 9,  WHITE);
    fill_circle(cx + 14, cy - 35, 10, 9,  WHITE);
    fill_circle(cx - 14, cy - 35, 7,  8,  GREEN);
    fill_circle(cx + 14, cy - 35, 7,  8,  GREEN);
    /* pupils */
    fill_circle(cx - 14, cy - 35, 3,  7,  BLACK);
    fill_circle(cx + 14, cy - 35, 3,  7,  BLACK);
    /* eye shine */
    plot_pixel(cx - 11, cy - 38, WHITE);
    plot_pixel(cx - 10, cy - 38, WHITE);
    plot_pixel(cx + 17, cy - 38, WHITE);
    plot_pixel(cx + 18, cy - 38, WHITE);

    /* ── nose ── */
    fill_triangle(cx - 4, cy - 25, cx + 4, cy - 25, cx, cy - 20, DARK_PINK);

    /* ── mouth ── */
    draw_line(cx,      cy - 20, cx - 6, cy - 14, BLACK);
    draw_line(cx,      cy - 20, cx + 6, cy - 14, BLACK);

    /* ── whiskers ── */
    draw_line(cx - 16, cy - 22, cx - 55, cy - 25, WHITE);
    draw_line(cx - 16, cy - 20, cx - 55, cy - 20, WHITE);
    draw_line(cx - 16, cy - 18, cx - 55, cy - 16, WHITE);
    draw_line(cx + 16, cy - 22, cx + 55, cy - 25, WHITE);
    draw_line(cx + 16, cy - 20, cx + 55, cy - 20, WHITE);
    draw_line(cx + 16, cy - 18, cx + 55, cy - 16, WHITE);
}

int main(void)
{
    /* wait for VGA to be ready */
   *(vga_ctrl + 1) = PIXEL_BASE;

    draw_cat(160, 130);

    while (1);
    return 0;
}