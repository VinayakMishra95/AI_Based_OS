#include "task.h"
#include "string.h"

static task_t tasks[MAX_TASKS];
static uint32_t next_pid = 1;
static int current_idx = 0;

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_spawn("kernel_idle", true);
    task_spawn("gui_compositor", true);
    task_spawn("ai_serial_daemon", true);
    tasks[0].state = TASK_RUNNING;
}

int task_spawn(const char* name, bool system_task) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            tasks[i].pid = next_pid++;
            strncpy(tasks[i].name, name, 23);
            tasks[i].name[23] = '\0';
            tasks[i].state = TASK_READY;
            tasks[i].ticks = 0;
            tasks[i].system_task = system_task;
            return (int)tasks[i].pid;
        }
    }
    return -1;
}

int task_kill(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid) {
            if (tasks[i].system_task) return -2; // Protect core kernel tasks
            tasks[i].state = TASK_UNUSED;
            return 0;
        }
    }
    return -1;
}

void task_tick_scheduler(void) {
    if (tasks[current_idx].state == TASK_RUNNING) {
        tasks[current_idx].ticks++;
        tasks[current_idx].state = TASK_READY;
    }
    for (int step = 1; step <= MAX_TASKS; step++) {
        int idx = (current_idx + step) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            current_idx = idx;
            tasks[idx].state = TASK_RUNNING;
            break;
        }
    }
}

const task_t* task_get(int index) {
    if (index < 0 || index >= MAX_TASKS) return NULL;
    if (tasks[index].state == TASK_UNUSED) return NULL;
    return &tasks[index];
}

int task_count(void) {
    int c = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED) c++;
    }
    return c;
}
