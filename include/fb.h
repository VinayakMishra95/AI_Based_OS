#ifndef FB_H
#define FB_H

#include "types.h"

void fb_init(BootInfo* info);
uint32_t fb_width(void);
uint32_t fb_height(void);
void fb_put_pixel(int32_t x, int32_t y, uint32_t color);
void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void fb_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void fb_fill_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t top_col, uint32_t bot_col);
void fb_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg, bool transparent_bg);
void fb_draw_string(int32_t x, int32_t y, const char* s, uint32_t fg, uint32_t bg, bool transparent_bg);
void fb_draw_cursor(int32_t x, int32_t y);
void fb_swap(void);

#endif
