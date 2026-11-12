#include "fb.h"
#include "font8x16.h"
#include "memory.h"
#include "string.h"

static uint32_t* lfb = NULL;
static uint32_t* backbuf = NULL;
static uint32_t  width = 1024;
static uint32_t  height = 768;
static uint32_t  pitch_px = 1024;

void fb_init(BootInfo* info) {
    lfb = (uint32_t*)info->fb_addr;
    width = info->fb_width;
    height = info->fb_height;
    pitch_px = info->fb_pitch / 4;

    backbuf = (uint32_t*)kmalloc(width * height * 4);
    if (backbuf) {
        memset(backbuf, 0, width * height * 4);
    } else {
        backbuf = lfb;
    }
}

uint32_t fb_width(void) { return width; }
uint32_t fb_height(void) { return height; }

void fb_put_pixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= width || (uint32_t)y >= height) return;
    backbuf[y * width + x] = color;
}

void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = (x + w > (int32_t)width) ? (int32_t)width : (x + w);
    int32_t y1 = (y + h > (int32_t)height) ? (int32_t)height : (y + h);

    for (int32_t py = y0; py < y1; py++) {
        uint32_t* row = &backbuf[py * (int32_t)width + x0];
        for (int32_t px = x0; px < x1; px++) {
            *row++ = color;
        }
    }
}

void fb_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    fb_fill_rect(x, y, w, 1, color);
    fb_fill_rect(x, y + h - 1, w, 1, color);
    fb_fill_rect(x, y, 1, h, color);
    fb_fill_rect(x + w - 1, y, 1, h, color);
}

void fb_fill_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t top_col, uint32_t bot_col) {
    if (h <= 0) return;
    uint8_t r1 = (top_col >> 16) & 0xFF, g1 = (top_col >> 8) & 0xFF, b1 = top_col & 0xFF;
    uint8_t r2 = (bot_col >> 16) & 0xFF, g2 = (bot_col >> 8) & 0xFF, b2 = bot_col & 0xFF;

    for (int32_t row = 0; row < h; row++) {
        uint32_t r = r1 + ((int32_t)(r2 - r1) * row) / h;
        uint32_t g = g1 + ((int32_t)(g2 - g1) * row) / h;
        uint32_t b = b1 + ((int32_t)(b2 - b1) * row) / h;
        uint32_t col = (r << 16) | (g << 8) | b;
        fb_fill_rect(x, y + row, w, 1, col);
    }
}

void fb_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg, bool transparent_bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font8x16[c - 32];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                fb_put_pixel(x + col, y + row, fg);
            } else if (!transparent_bg) {
                fb_put_pixel(x + col, y + row, bg);
            }
        }
    }
}

void fb_draw_string(int32_t x, int32_t y, const char* s, uint32_t fg, uint32_t bg, bool transparent_bg) {
    int32_t cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += 16;
        } else {
            fb_draw_char(cx, y, *s, fg, bg, transparent_bg);
            cx += 8;
        }
        s++;
    }
}

static const uint8_t cursor_bitmap[14][10] = {
    {1,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,1,1,1,1,1},
    {1,2,2,1,2,1,0,0,0,0},
    {1,2,1,0,1,2,1,0,0,0},
    {1,1,0,0,1,2,1,0,0,0},
    {0,0,0,0,0,1,0,0,0,0}
};

void fb_draw_cursor(int32_t x, int32_t y) {
    for (int r = 0; r < 14; r++) {
        for (int c = 0; c < 10; c++) {
            uint8_t px = cursor_bitmap[r][c];
            if (px == 1) fb_put_pixel(x + c, y + r, 0x0F172A);
            else if (px == 2) fb_put_pixel(x + c, y + r, 0xFFFFFF);
        }
    }
}

void fb_swap(void) {
    if (!backbuf || backbuf == lfb) return;
    if (pitch_px == width) {
        memcpy(lfb, backbuf, width * height * 4);
    } else {
        for (uint32_t y = 0; y < height; y++) {
            memcpy(&lfb[y * pitch_px], &backbuf[y * width], width * 4);
        }
    }
}
