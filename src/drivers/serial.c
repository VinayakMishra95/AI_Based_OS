#include "serial.h"
#include "io.h"
#include "idt.h"

#define COM1 0x3F8
#define SERIAL_BUF_SIZE 2048

static volatile char rx_buf[SERIAL_BUF_SIZE];
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;

static void serial_push(char c) {
    uint32_t next = (rx_head + 1) % SERIAL_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

static void serial_irq_handler(registers_t* regs) {
    (void)regs;
    while (inb(COM1 + 5) & 0x01) {
        char c = (char)inb(COM1);
        serial_push(c);
    }
}

void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB
    outb(COM1 + 0, 0x01); // 115200 baud (low byte)
    outb(COM1 + 1, 0x00); // (high byte)
    outb(COM1 + 3, 0x03); // 8N1
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set

    irq_register_handler(4, serial_irq_handler);
    outb(COM1 + 1, 0x01); // Enable Received Data Available interrupt
}

void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putc('\r');
        }
        serial_putc(*str++);
    }
}

bool serial_has_data(void) {
    if (inb(COM1 + 5) & 0x01) {
        while (inb(COM1 + 5) & 0x01) {
            serial_push((char)inb(COM1));
        }
    }
    return rx_head != rx_tail;
}

int serial_getc_nonblock(void) {
    serial_has_data();
    if (rx_head == rx_tail) return -1;
    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % SERIAL_BUF_SIZE;
    return (uint8_t)c;
}
