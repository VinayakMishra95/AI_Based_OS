#include "memory.h"
#include "string.h"

typedef struct mem_block {
    uint32_t size;
    bool free;
    struct mem_block* next;
} mem_block_t;

static mem_block_t* heap_head = NULL;
static uint32_t heap_total_bytes = 0;

void memory_init(uint32_t start_addr, uint32_t total_size) {
    heap_head = (mem_block_t*)start_addr;
    heap_head->size = total_size - sizeof(mem_block_t);
    heap_head->free = true;
    heap_head->next = NULL;
    heap_total_bytes = total_size;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7) & ~7u; // 8-byte align

    mem_block_t* curr = heap_head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size >= size + sizeof(mem_block_t) + 16) {
                mem_block_t* next_blk = (mem_block_t*)((uint8_t*)curr + sizeof(mem_block_t) + size);
                next_blk->size = curr->size - size - sizeof(mem_block_t);
                next_blk->free = true;
                next_blk->next = curr->next;
                curr->size = size;
                curr->next = next_blk;
            }
            curr->free = false;
            return (void*)((uint8_t*)curr + sizeof(mem_block_t));
        }
        curr = curr->next;
    }
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    mem_block_t* blk = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    blk->free = true;

    // Coalesce adjacent free blocks
    mem_block_t* curr = heap_head;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += sizeof(mem_block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

uint32_t memory_get_used_kb(void) {
    uint32_t used = 0;
    mem_block_t* curr = heap_head;
    while (curr) {
        if (!curr->free) {
            used += curr->size + sizeof(mem_block_t);
        }
        curr = curr->next;
    }
    return used / 1024;
}

uint32_t memory_get_total_kb(void) {
    return heap_total_bytes / 1024;
}
