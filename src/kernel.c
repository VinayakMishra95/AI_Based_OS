#include "types.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "serial.h"
#include "timer.h"
#include "memory.h"
#include "fs.h"
#include "task.h"
#include "fb.h"
#include "keyboard.h"
#include "mouse.h"
#include "gui.h"

void kmain(BootInfo* boot_info);

__attribute__((section(".text.entry")))
void _start(BootInfo* boot_info) {
    kmain(boot_info);
    for (;;) {
        cli();
        hlt();
    }
}

void kmain(BootInfo* boot_info) {
    gdt_init();
    idt_init();
    serial_init();
    serial_write("\n[AetherOS] Kernel booting...\n");

    memory_init(0x400000, 16 * 1024 * 1024);
    ramfs_init();
    task_init();
    fb_init(boot_info);

    timer_init(100);
    keyboard_init();
    mouse_init((int32_t)boot_info->fb_width, (int32_t)boot_info->fb_height);
    gui_init();

    sti();
    serial_write("[AetherOS] GUI & Interrupts active.\n");

    uint32_t last_tick = 0;
    for (;;) {
        while (keyboard_has_char()) {
            gui_handle_key(keyboard_getc());
        }
        while (serial_has_data()) {
            int ch = serial_getc_nonblock();
            if (ch >= 0) {
                gui_handle_serial_byte((char)ch);
            }
        }

        uint32_t now = timer_get_ticks();
        if (now != last_tick) {
            last_tick = now;
            task_tick_scheduler();
            gui_update_and_draw();
        }
        hlt();
    }
}
