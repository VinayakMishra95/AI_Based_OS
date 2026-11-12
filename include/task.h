#ifndef TASK_H
#define TASK_H

#include "types.h"

#define MAX_TASKS 8

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING
} task_state_t;

typedef struct {
    uint32_t pid;
    char name[24];
    task_state_t state;
    uint32_t ticks;
    bool system_task;
} task_t;

void task_init(void);
int  task_spawn(const char* name, bool system_task);
int  task_kill(uint32_t pid);
void task_tick_scheduler(void);
const task_t* task_get(int index);
int  task_count(void);

#endif
