#include "gui.h"
#include "fb.h"
#include "mouse.h"
#include "timer.h"
#include "memory.h"
#include "fs.h"
#include "task.h"
#include "serial.h"
#include "string.h"

#define TERM_COLS 66
#define TERM_ROWS 22
#define CMD_MAX   128
#define AI_LINE_MAX 256

typedef struct {
    int32_t x, y, w, h;
    bool visible;
    bool dragging;
    int32_t drag_off_x, drag_off_y;
    const char* title;
    uint32_t accent;
} window_t;

static window_t win_term;
static window_t win_sys;
static window_t win_ai;
static int active_win = 0; // 0=term, 1=sys, 2=ai

static char term_lines[TERM_ROWS][TERM_COLS + 1];
static int  term_line_count = 0;
static char cmd_buf[CMD_MAX];
static int  cmd_len = 0;

static char ai_last_prompt[128] = "No prompt sent yet. Try: ai create todo.txt with buy milk";
static char ai_last_reply[512]  = "Bridge ready. AI can inspect OS state & execute commands.";
static bool ai_waiting = false;
static char serial_rx_line[AI_LINE_MAX];
static int  serial_rx_len = 0;

static void term_scroll(void) {
    for (int i = 0; i < TERM_ROWS - 1; i++) {
        memcpy(term_lines[i], term_lines[i + 1], TERM_COLS + 1);
    }
    memset(term_lines[TERM_ROWS - 1], 0, TERM_COLS + 1);
}

static void term_print_line(const char* s) {
    if (term_line_count >= TERM_ROWS) {
        term_scroll();
        term_line_count = TERM_ROWS - 1;
    }
    strncpy(term_lines[term_line_count], s, TERM_COLS);
    term_lines[term_line_count][TERM_COLS] = '\0';
    term_line_count++;
}

static void term_print_wrapped(const char* s) {
    char line[TERM_COLS + 1];
    int idx = 0;
    while (*s) {
        if (*s == '\n' || idx >= TERM_COLS) {
            line[idx] = '\0';
            term_print_line(line);
            idx = 0;
            if (*s == '\n') s++;
        } else {
            line[idx++] = *s++;
        }
    }
    if (idx > 0) {
        line[idx] = '\0';
        term_print_line(line);
    }
}

static int parse_int(const char** p) {
    const char* s = *p;
    while (*s == ' ') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    *p = s;
    return val * sign;
}

static uint32_t parse_hex(const char* s) {
    while (*s == ' ') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t val = 0;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9') val = (val << 4) | (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = (val << 4) | (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = (val << 4) | (uint32_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

static void execute_command(const char* cmd) {
    char echo[TERM_COLS + 1];
    strcpy(echo, "aether> ");
    strncpy(echo + 8, cmd, TERM_COLS - 8);
    echo[TERM_COLS] = '\0';
    term_print_line(echo);

    if (cmd[0] == '\0') return;

    if (strcmp(cmd, "help") == 0) {
        term_print_line("Files:   ls, cat <f>, touch <f>, write <f> <txt>, rm <f>, sync");
        term_print_line("Tasks:   ps, spawn <name>, kill <pid>");
        term_print_line("System:  mem, uptime, uname, calc <a op b>, hexdump <0xADDR>");
        term_print_line("AI/Misc: ai <natural language task or question>, clear");
    } else if (strcmp(cmd, "clear") == 0) {
        memset(term_lines, 0, sizeof(term_lines));
        term_line_count = 0;
    } else if (strcmp(cmd, "uname") == 0) {
        term_print_line("AetherOS 0.2 i386 (VBE 32bpp + ATA PIO + AI Serial Bridge)");
    } else if (strcmp(cmd, "ls") == 0) {
        char row[TERM_COLS + 1];
        char sz[16];
        for (int i = 0; i < RAMFS_MAX_FILES; i++) {
            const ramfs_file_t* f = ramfs_get(i);
            if (f) {
                itoa((int32_t)f->size, sz, 10);
                strcpy(row, "  ");
                strcat(row, f->name);
                strcat(row, " (");
                strcat(row, sz);
                strcat(row, " B)");
                term_print_line(row);
            }
        }
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char* content = ramfs_read(cmd + 4);
        if (content) term_print_wrapped(content);
        else term_print_line("File not found.");
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        if (ramfs_create(cmd + 6) == 0) term_print_line("Created file.");
        else term_print_line("Error creating file.");
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        if (ramfs_delete(cmd + 3) == 0) term_print_line("Removed file.");
        else term_print_line("File not found.");
    } else if (strncmp(cmd, "write ", 6) == 0) {
        const char* rest = cmd + 6;
        char fname[32];
        int i = 0;
        while (*rest && *rest != ' ' && i < 31) {
            fname[i++] = *rest++;
        }
        fname[i] = '\0';
        if (*rest == ' ') rest++;
        if (ramfs_write(fname, rest) == 0) term_print_line("Saved in RAMFS (run 'sync' to persist to disk).");
        else term_print_line("Write failed.");
    } else if (strcmp(cmd, "sync") == 0) {
        if (ramfs_sync() == 0) term_print_line("Filesystem synced to ATA disk (LBA 200).");
        else term_print_line("ATA disk sync failed.");
    } else if (strcmp(cmd, "ps") == 0) {
        char row[TERM_COLS + 1], num[16];
        term_print_line("PID  STATE    TICKS   NAME");
        for (int i = 0; i < MAX_TASKS; i++) {
            const task_t* t = task_get(i);
            if (t) {
                itoa((int32_t)t->pid, num, 10);
                strcpy(row, num);
                strcat(row, "    ");
                strcat(row, t->state == TASK_RUNNING ? "RUN      " : "READY    ");
                itoa((int32_t)t->ticks, num, 10);
                strcat(row, num);
                strcat(row, "   ");
                strcat(row, t->name);
                term_print_line(row);
            }
        }
    } else if (strncmp(cmd, "spawn ", 6) == 0) {
        int pid = task_spawn(cmd + 6, false);
        if (pid > 0) {
            char msg[48], num[16];
            itoa(pid, num, 10);
            strcpy(msg, "Spawned task PID ");
            strcat(msg, num);
            term_print_line(msg);
        } else {
            term_print_line("Task table full (max 8).");
        }
    } else if (strncmp(cmd, "kill ", 5) == 0) {
        const char* p = cmd + 5;
        int pid = parse_int(&p);
        int res = task_kill((uint32_t)pid);
        if (res == 0) term_print_line("Task terminated.");
        else if (res == -2) term_print_line("Cannot kill core kernel task.");
        else term_print_line("PID not found.");
    } else if (strncmp(cmd, "calc ", 5) == 0) {
        const char* p = cmd + 5;
        int a = parse_int(&p);
        while (*p == ' ') p++;
        char op = *p++;
        int b = parse_int(&p);
        int res = 0;
        bool ok = true;
        if (op == '+') res = a + b;
        else if (op == '-') res = a - b;
        else if (op == '*') res = a * b;
        else if (op == '/' && b != 0) res = a / b;
        else if (op == '%' && b != 0) res = a % b;
        else ok = false;

        if (ok) {
            char msg[48], num[16];
            itoa(res, num, 10);
            strcpy(msg, "= ");
            strcat(msg, num);
            term_print_line(msg);
        } else {
            term_print_line("Usage: calc <int> <+|-|*|/|%> <int>");
        }
    } else if (strncmp(cmd, "hexdump ", 8) == 0) {
        uint32_t addr = parse_hex(cmd + 8);
        const uint8_t* ptr = (const uint8_t*)addr;
        const char* hex = "0123456789ABCDEF";
        for (int row_i = 0; row_i < 4; row_i++) {
            char line[TERM_COLS + 1];
            int pos = 0;
            uint32_t line_addr = addr + (uint32_t)(row_i * 16);
            for (int shift = 28; shift >= 0; shift -= 4) {
                line[pos++] = hex[(line_addr >> shift) & 0xF];
            }
            line[pos++] = ':';
            line[pos++] = ' ';
            for (int b = 0; b < 16; b++) {
                uint8_t v = ptr[row_i * 16 + b];
                line[pos++] = hex[(v >> 4) & 0xF];
                line[pos++] = hex[v & 0xF];
                line[pos++] = ' ';
            }
            line[pos] = '\0';
            term_print_line(line);
        }
    } else if (strcmp(cmd, "mem") == 0) {
        char buf[64], n1[16], n2[16];
        itoa((int32_t)memory_get_used_kb(), n1, 10);
        itoa((int32_t)memory_get_total_kb(), n2, 10);
        strcpy(buf, "Heap Used: ");
        strcat(buf, n1);
        strcat(buf, " KB / ");
        strcat(buf, n2);
        strcat(buf, " KB");
        term_print_line(buf);
    } else if (strcmp(cmd, "uptime") == 0) {
        char buf[64], n1[16];
        itoa((int32_t)timer_get_seconds(), n1, 10);
        strcpy(buf, "Uptime: ");
        strcat(buf, n1);
        strcat(buf, " seconds");
        term_print_line(buf);
    } else if (strncmp(cmd, "ai ", 3) == 0) {
        const char* prompt = cmd + 3;
        strncpy(ai_last_prompt, prompt, sizeof(ai_last_prompt) - 1);
        strcpy(ai_last_reply, "Thinking via COM1 AI Bridge...");
        ai_waiting = true;
        win_ai.visible = true;

        serial_write("[AI_REQ]");
        serial_write(prompt);
        serial_write("\n");
        term_print_line("[AI] Sent request to host bridge.");
    } else {
        term_print_line("Unknown command. Type 'help'.");
    }
}

void gui_init(void) {
    uint32_t w = fb_width();
    uint32_t h = fb_height();

    win_term = (window_t){ 28, 48, 560, 420, true, false, 0, 0, "Terminal - AetherShell", 0x38BDF8 };
    win_sys  = (window_t){ (int32_t)w - 390, 48, 360, 240, true, false, 0, 0, "System Monitor", 0x34D399 };
    win_ai   = (window_t){ (int32_t)w - 390, 310, 360, (int32_t)h - 380, true, false, 0, 0, "AI Assistant (COM1)", 0xA78BFA };

    memset(term_lines, 0, sizeof(term_lines));
    term_print_line("AetherOS v0.2 - x86 Bare-Metal C OS (ATA + Tasks + AI)");
    term_print_line(ramfs_is_persisted() ? "Disk: Loaded saved filesystem from ATA LBA 200." : "Disk: Default RAMFS mounted (run 'sync' to save).");
    term_print_line("Type 'help' for commands.");
}

void gui_handle_key(char c) {
    if (!win_term.visible) {
        win_term.visible = true;
    }
    active_win = 0;

    if (c == '\n') {
        cmd_buf[cmd_len] = '\0';
        execute_command(cmd_buf);
        cmd_len = 0;
        cmd_buf[0] = '\0';
    } else if (c == '\b') {
        if (cmd_len > 0) {
            cmd_len--;
            cmd_buf[cmd_len] = '\0';
        }
    } else if (c >= 32 && c <= 126) {
        if (cmd_len < CMD_MAX - 1) {
            cmd_buf[cmd_len++] = c;
            cmd_buf[cmd_len] = '\0';
        }
    }
}

void gui_handle_serial_byte(char c) {
    if (c == '\r') return;
    if (c == '\n') {
        serial_rx_line[serial_rx_len] = '\0';
        if (strncmp(serial_rx_line, "[AI_RES]", 8) == 0) {
            strncpy(ai_last_reply, serial_rx_line + 8, sizeof(ai_last_reply) - 1);
            ai_last_reply[sizeof(ai_last_reply) - 1] = '\0';
            ai_waiting = false;
            term_print_line("[AI] Reply ready in AI Assistant.");
        } else if (strncmp(serial_rx_line, "[AI_CMD]", 8) == 0) {
            execute_command(serial_rx_line + 8);
        }
        serial_rx_len = 0;
    } else if (serial_rx_len < AI_LINE_MAX - 1) {
        serial_rx_line[serial_rx_len++] = c;
    }
}

static bool point_in_rect(int32_t px, int32_t py, int32_t x, int32_t y, int32_t w, int32_t h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void update_window_drag(window_t* win, int id, int32_t mx, int32_t my, bool down, bool clicked) {
    if (!win->visible) return;
    if (clicked) {
        if (point_in_rect(mx, my, win->x + win->w - 24, win->y + 6, 16, 16)) {
            win->visible = false;
            return;
        }
        if (point_in_rect(mx, my, win->x, win->y, win->w, 28)) {
            active_win = id;
            win->dragging = true;
            win->drag_off_x = mx - win->x;
            win->drag_off_y = my - win->y;
        } else if (point_in_rect(mx, my, win->x, win->y, win->w, win->h)) {
            active_win = id;
        }
    }
    if (!down) {
        win->dragging = false;
    } else if (win->dragging) {
        win->x = mx - win->drag_off_x;
        win->y = my - win->drag_off_y;
        if (win->y < 32) win->y = 32;
    }
}

static void draw_window_frame(const window_t* win, bool is_active) {
    fb_fill_rect(win->x + 4, win->y + 4, win->w, win->h, 0x090D16);
    fb_fill_rect(win->x, win->y, win->w, win->h, 0x1E293B);
    fb_fill_rect(win->x, win->y, win->w, 28, is_active ? 0x334155 : 0x263244);
    fb_fill_rect(win->x, win->y, 4, 28, win->accent);
    fb_draw_rect(win->x, win->y, win->w, win->h, is_active ? win->accent : 0x475569);
    fb_draw_string(win->x + 12, win->y + 6, win->title, 0xF8FAFC, 0, true);
    fb_fill_rect(win->x + win->w - 24, win->y + 6, 16, 16, 0xEF4444);
    fb_draw_char(win->x + win->w - 20, win->y + 6, 'x', 0xFFFFFF, 0, true);
}

static void draw_wrapped_text(int32_t x, int32_t y, int32_t max_chars, int32_t max_lines, const char* s, uint32_t color) {
    int32_t cx = 0;
    int32_t cy = 0;
    while (*s && cy < max_lines) {
        if (*s == '\n' || cx >= max_chars) {
            cx = 0;
            cy++;
            if (*s == '\n') s++;
        } else {
            fb_draw_char(x + cx * 8, y + cy * 16, *s, color, 0, true);
            cx++;
            s++;
        }
    }
}

void gui_update_and_draw(void) {
    uint32_t w = fb_width();
    uint32_t h = fb_height();
    int32_t mx = mouse_get_x();
    int32_t my = mouse_get_y();
    bool down = mouse_left_down();
    bool clicked = mouse_left_clicked();

    if (clicked && my >= (int32_t)h - 42) {
        int32_t dock_x = (int32_t)w / 2 - 210;
        if (point_in_rect(mx, my, dock_x, (int32_t)h - 38, 130, 30)) {
            win_term.visible = !win_term.visible;
            active_win = 0;
        } else if (point_in_rect(mx, my, dock_x + 140, (int32_t)h - 38, 130, 30)) {
            win_sys.visible = !win_sys.visible;
            active_win = 1;
        } else if (point_in_rect(mx, my, dock_x + 280, (int32_t)h - 38, 130, 30)) {
            win_ai.visible = !win_ai.visible;
            active_win = 2;
        }
    }

    update_window_drag(&win_term, 0, mx, my, down, clicked);
    update_window_drag(&win_sys,  1, mx, my, down, clicked);
    update_window_drag(&win_ai,   2, mx, my, down, clicked);

    fb_fill_gradient_v(0, 0, (int32_t)w, (int32_t)h, 0x0F172A, 0x1E1B4B);

    fb_fill_rect(0, 0, (int32_t)w, 30, 0x090D16);
    fb_fill_rect(0, 29, (int32_t)w, 1, 0x334155);
    fb_draw_string(14, 7, "AetherOS v0.2 (x86 Bare-Metal C)", 0x38BDF8, 0, true);

    char stat[64], n1[16], n2[16];
    itoa((int32_t)memory_get_used_kb(), n1, 10);
    itoa((int32_t)timer_get_seconds(), n2, 10);
    strcpy(stat, "RAM: ");
    strcat(stat, n1);
    strcat(stat, " KB  |  Uptime: ");
    strcat(stat, n2);
    strcat(stat, "s");
    fb_draw_string((int32_t)w - 280, 7, stat, 0xCBD5E1, 0, true);

    if (win_sys.visible) {
        draw_window_frame(&win_sys, active_win == 1);
        int32_t sx = win_sys.x + 14;
        int32_t sy = win_sys.y + 40;
        char line[64], num[16];

        fb_draw_string(sx, sy, "Arch: x86 Protected | ATA PIO", 0xE2E8F0, 0, true);
        itoa((int32_t)w, num, 10);
        strcpy(line, "Display: ");
        strcat(line, num);
        strcat(line, "x");
        itoa((int32_t)h, num, 10);
        strcat(line, num);
        strcat(line, " (32bpp VBE)");
        fb_draw_string(sx, sy + 24, line, 0x94A3B8, 0, true);

        itoa((int32_t)timer_get_ticks(), num, 10);
        strcpy(line, "PIT Ticks: ");
        strcat(line, num);
        strcat(line, " | Tasks: ");
        itoa(task_count(), num, 10);
        strcat(line, num);
        fb_draw_string(sx, sy + 48, line, 0x34D399, 0, true);

        uint32_t used = memory_get_used_kb();
        uint32_t total = memory_get_total_kb();
        itoa((int32_t)used, num, 10);
        strcpy(line, "Kernel Heap: ");
        strcat(line, num);
        strcat(line, " / ");
        itoa((int32_t)total, num, 10);
        strcat(line, num);
        strcat(line, " KB");
        fb_draw_string(sx, sy + 76, line, 0xE2E8F0, 0, true);

        fb_fill_rect(sx, sy + 98, 320, 14, 0x0F172A);
        int32_t bar_w = (int32_t)((used * 320) / (total ? total : 1));
        if (bar_w < 6) bar_w = 6;
        fb_fill_rect(sx, sy + 98, bar_w, 14, 0x34D399);

        itoa(ramfs_count(), num, 10);
        strcpy(line, "Files: ");
        strcat(line, num);
        strcat(line, ramfs_is_persisted() ? " (Saved on ATA)" : " (Unsaved RAM)");
        fb_draw_string(sx, sy + 128, line, 0x94A3B8, 0, true);
    }

    if (win_ai.visible) {
        draw_window_frame(&win_ai, active_win == 2);
        int32_t ax = win_ai.x + 14;
        int32_t ay = win_ai.y + 40;
        fb_draw_string(ax, ay, "Prompt:", 0xA78BFA, 0, true);
        draw_wrapped_text(ax, ay + 20, 41, 4, ai_last_prompt, 0xF8FAFC);
        fb_fill_rect(ax, ay + 90, win_ai.w - 28, 1, 0x334155);
        fb_draw_string(ax, ay + 102, ai_waiting ? "Status: Waiting on COM1..." : "Response:", 0x38BDF8, 0, true);
        draw_wrapped_text(ax, ay + 124, 41, 14, ai_last_reply, 0xCBD5E1);
    }

    if (win_term.visible) {
        draw_window_frame(&win_term, active_win == 0);
        int32_t tx = win_term.x + 12;
        int32_t ty = win_term.y + 38;
        for (int i = 0; i < term_line_count; i++) {
            fb_draw_string(tx, ty + i * 16, term_lines[i], 0xE2E8F0, 0, true);
        }
        int32_t py = ty + term_line_count * 16;
        if (py < win_term.y + win_term.h - 20) {
            fb_draw_string(tx, py, "aether> ", 0x38BDF8, 0, true);
            fb_draw_string(tx + 64, py, cmd_buf, 0xFFFFFF, 0, true);
            if ((timer_get_ticks() / 50) % 2 == 0) {
                fb_fill_rect(tx + 64 + cmd_len * 8, py, 8, 14, 0x38BDF8);
            }
        }
    }

    int32_t dock_x = (int32_t)w / 2 - 210;
    fb_fill_rect(dock_x - 10, (int32_t)h - 44, 430, 40, 0x090D16);
    fb_draw_rect(dock_x - 10, (int32_t)h - 44, 430, 40, 0x334155);

    fb_fill_rect(dock_x, (int32_t)h - 38, 130, 28, win_term.visible ? 0x1E293B : 0x0F172A);
    fb_draw_rect(dock_x, (int32_t)h - 38, 130, 28, 0x38BDF8);
    fb_draw_string(dock_x + 32, (int32_t)h - 32, "Terminal", 0xF8FAFC, 0, true);

    fb_fill_rect(dock_x + 140, (int32_t)h - 38, 130, 28, win_sys.visible ? 0x1E293B : 0x0F172A);
    fb_draw_rect(dock_x + 140, (int32_t)h - 38, 130, 28, 0x34D399);
    fb_draw_string(dock_x + 168, (int32_t)h - 32, "SysMonitor", 0xF8FAFC, 0, true);

    fb_fill_rect(dock_x + 280, (int32_t)h - 38, 130, 28, win_ai.visible ? 0x1E293B : 0x0F172A);
    fb_draw_rect(dock_x + 280, (int32_t)h - 38, 130, 28, 0xA78BFA);
    fb_draw_string(dock_x + 308, (int32_t)h - 32, "AI Bridge", 0xF8FAFC, 0, true);

    fb_draw_cursor(mx, my);
    fb_swap();
}
