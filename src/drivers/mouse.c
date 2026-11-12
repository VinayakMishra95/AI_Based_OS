#include "mouse.h"
#include "idt.h"
#include "io.h"

static int32_t mouse_x = 320;
static int32_t mouse_y = 240;
static int32_t screen_w = 1024;
static int32_t screen_h = 768;
static uint8_t mouse_cycle = 0;
static int8_t  mouse_bytes[3];
static bool    left_btn = false;
static bool    prev_left_btn = false;
static volatile bool click_event = false;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, val);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

static void mouse_callback(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(0x64);
    if (!(status & 0x01) || !(status & 0x20)) {
        return;
    }

    int8_t data = (int8_t)inb(0x60);
    if (mouse_cycle == 0) {
        if (!(data & 0x08)) return;
        mouse_bytes[0] = data;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = data;
        mouse_cycle = 2;
    } else {
        mouse_bytes[2] = data;
        mouse_cycle = 0;

        uint8_t flags = (uint8_t)mouse_bytes[0];
        if (flags & 0xC0) return;

        int32_t dx = mouse_bytes[1];
        int32_t dy = mouse_bytes[2];

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= screen_w) mouse_x = screen_w - 1;
        if (mouse_y >= screen_h) mouse_y = screen_h - 1;

        prev_left_btn = left_btn;
        left_btn = (flags & 0x01) != 0;
        if (left_btn && !prev_left_btn) {
            click_event = true;
        }
    }
}

void mouse_init(int32_t max_w, int32_t max_h) {
    screen_w = max_w;
    screen_h = max_h;
    mouse_x = max_w / 2;
    mouse_y = max_h / 2;

    mouse_wait(1);
    outb(0x64, 0xA8);

    mouse_wait(1);
    outb(0x64, 0x20);
    uint8_t status = (uint8_t)(mouse_read() | 2);
    status &= (uint8_t)~0x20;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    irq_register_handler(12, mouse_callback);
}

int32_t mouse_get_x(void) { return mouse_x; }
int32_t mouse_get_y(void) { return mouse_y; }
bool mouse_left_down(void) { return left_btn; }

bool mouse_left_clicked(void) {
    if (click_event) {
        click_event = false;
        return true;
    }
    return false;
}
