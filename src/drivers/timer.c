#include "timer.h"
#include "idt.h"
#include "io.h"

static volatile uint32_t tick_count = 0;
static uint32_t timer_freq = 100;

static void timer_callback(registers_t* regs) {
    (void)regs;
    tick_count++;
}

void timer_init(uint32_t freq_hz) {
    timer_freq = freq_hz;
    irq_register_handler(0, timer_callback);

    uint32_t divisor = 1193180 / freq_hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_seconds(void) {
    return tick_count / timer_freq;
}
