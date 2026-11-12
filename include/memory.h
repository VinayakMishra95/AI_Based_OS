#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

void memory_init(uint32_t start_addr, uint32_t total_size);
void* kmalloc(size_t size);
void kfree(void* ptr);
uint32_t memory_get_used_kb(void);
uint32_t memory_get_total_kb(void);

#endif
