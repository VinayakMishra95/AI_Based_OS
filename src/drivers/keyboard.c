#include "keyboard.h"
#include "idt.h"
#include "io.h"

#define KBD_BUF_SIZE 256

static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static bool shift_pressed = false;
static bool caps_lock = false;

static const char scancode_lower[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_upper[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static void kbd_push(char c) {
    uint32_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) {
        shift_pressed = true;
        return;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shift_pressed = false;
        return;
    }
    if (sc == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    if (sc & 0x80) {
        return; // Key release
    }

    if (sc < 128) {
        char c = shift_pressed ? scancode_upper[sc] : scancode_lower[sc];
        if (caps_lock && c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else if (caps_lock && c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != 0) {
            kbd_push(c);
        }
    }
}

void keyboard_init(void) {
    irq_register_handler(1, keyboard_callback);
}

bool keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

char keyboard_getc(void) {
    if (kbd_head == kbd_tail) return 0;
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}
