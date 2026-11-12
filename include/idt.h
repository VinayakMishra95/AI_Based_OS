#ifndef IDT_H
#define IDT_H

#include "types.h"

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

typedef void (*irq_handler_t)(registers_t*);

void idt_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);

#endif
