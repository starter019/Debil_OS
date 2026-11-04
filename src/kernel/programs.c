/* DEBIL OS - built-in programs: hello, snake, task manager, crash tester, calc */
#include "programs.h"
#include "console.h"
#include "input.h"
#include "timer.h"
#include "string.h"
#include "fs.h"
#include "cpu.h"
#include "mem.h"
#include "pci.h"
#include "panic.h"
#include "shell.h"
#include "io.h"

typedef struct {
    const char *name;
    const char *description;
    void (*entry)(void);
} program_entry_t;

static void prog_hello(void);
static void prog_snake(void);
static void prog_tasks(void);
static void prog_crash(void);
static void prog_calc(void);

static const program_entry_t g_programs[] = {
    { "hello.debil", "Hello / welcome demo",                       prog_hello },
    { "snake.debil", "Snake game - arrows/WASD, P pause, ESC quit", prog_snake },
    { "tasks.debil", "Live task manager - CPU, RAM, GPU, files",    prog_tasks },
    { "crash.debil", "Interactive CPU/RAM/GPU crash + reboot test", prog_crash },
    { "calc.debil",  "Tiny integer calculator",                     prog_calc  },
};

#define PROGRAM_COUNT ((int)(sizeof(g_programs) / sizeof(g_programs[0])))

static const char *g_current_program = "none";

/* ================================================================== */
/* Program registry helpers                                           */
/* ================================================================== */

void programs_list(void)
{
    console_set_color(COLOR_TITLE);
    console_printf("%-16s %s\n", "PROGRAM", "DESCRIPTION");
    console_set_color(COLOR_TEXT);
    for (int i = 0; i < PROGRAM_COUNT; i++) {
        console_printf("%-16s %s\n", g_programs[i].name, g_programs[i].description);
    }
    console_write_line("");
    console_write_line("Run one with: start <name>   (the .debil suffix is optional)");
    console_write_line("You can also create your own .debil text scripts with 'edit'.");
}

static const program_entry_t *find_program(const char *name)
{
    for (int i = 0; i < PROGRAM_COUNT; i++) {
        if (strcasecmp(g_programs[i].name, name) == 0) return &g_programs[i];
    }
    return 0;
}

/* ================================================================== */
/* .debil text script interpreter                                     */
/* ================================================================== */

static void run_script(fs_file_t *f)
{
    console_set_color(COLOR_TITLE);
    console_printf("Running .debil script: %s\n", f->name);
    console_set_color(COLOR_TEXT);

    const char *p = (const char *)f->data;
    char line[SHELL_MAX_LINE];

    while (*p) {
        int n = 0;
        while (*p && *p != '\n' && n < SHELL_MAX_LINE - 1) line[n++] = *p++;
        if (*p == '\n') p++;
        line[n] = 0;
        str_trim(line);

        if (!line[0] || line[0] == '#') continue;

        if (strncasecmp(line, "print ", 6) == 0) {
            console_write_line(line + 6);
        } else if (strncasecmp(line, "echo ", 5) == 0) {
            console_write_line(line + 5);
        } else if (strcasecmp(line, "clear") == 0) {
            console_clear();
        } else if (strncasecmp(line, "sleep ", 6) == 0) {
            uint32_t ms = (uint32_t)atoi(line + 6);
            if (ms > 60000) ms = 60000;
            timer_sleep_ms(ms);
        } else if (strcasecmp(line, "pause") == 0) {
            console_write_line("[script] press any key...");
            input_wait();
        } else if (strncasecmp(line, "crash ", 6) == 0) {
            const char *m = line + 6;
            if (strcasecmp(m, "cpu") == 0) crash_trigger(CRASH_CPU);
            else if (strcasecmp(m, "ram") == 0) crash_trigger(CRASH_RAM);
            else if (strcasecmp(m, "gpu") == 0) crash_trigger(CRASH_GPU);
            else console_write_line("[script] crash: mode must be cpu, ram or gpu");
        } else if (strcasecmp(line, "reboot") == 0) {
            machine_reboot();
        } else if (strcasecmp(line, "shutdown") == 0) {
            machine_shutdown();
        } else {
            console_printf("[script] unknown command: %s\n", line);
        }
    }

    console_set_color(COLOR_OK);
    console_write_line("Script finished.");
    console_set_color(COLOR_TEXT);
}

void programs_run(const char *name)
{
    if (!name || !*name) {
        console_write_line("usage: start <program.debil>");
        return;
    }

    char norm[64];
    strncpy(norm, name, sizeof(norm) - 1);
    norm[sizeof(norm) - 1] = 0;
    str_trim(norm);

    if (!strchr(norm, '.')) {
        if (strlen(norm) + 6 < sizeof(norm)) strcat(norm, ".debil");
    }

    const program_entry_t *p = find_program(norm);
    if (p) {
        g_current_program = p->name;
        p->entry();
        g_current_program = "none";
        return;
    }

    fs_file_t *f = fs_find(norm);
    if (f) {
        if (ends_with(f->name, ".debil")) {
            g_current_program = f->name;
            run_script(f);
            g_current_program = "none";
        } else {
            console_set_color(COLOR_ERROR);
            console_printf("%s is not a .debil program. Try 'cat %s'.\n", f->name, f->name);
            console_set_color(COLOR_TEXT);
        }
        return;
    }

    console_set_color(COLOR_ERROR);
    console_printf("start: program '%s' not found.\n", norm);
    console_set_color(COLOR_TEXT);
    console_write_line("Type 'programs' to see the installed programs.");
}

/* ================================================================== */
/* hello                                                              */
/* ================================================================== */

static void prog_hello(void)
{
    console_clear();
    console_set_color(COLOR_TITLE);
    console_write_line("Hello from a program running on DEBIL OS!");
    console_set_color(COLOR_TEXT);
    console_write_line("");
    console_printf("CPU       : %s\n", cpu_brand());
    console_printf("RAM       : %u MB\n", mem_total_kb() / 1024u);
    console_printf("Uptime    : %u seconds\n", timer_uptime_seconds());
    console_printf("Program   : %s\n", g_current_program);
    console_write_line("");
    console_write_line("This program is a function compiled into the kernel and launched");
    console_write_line("through the .debil program registry - no ELF loader required.");
    console_write_line("");
    console_write_line("Press Enter to return to the shell.");
    while (input_wait() != KEY_ENTER) { }
    console_clear();
}

/* ================================================================== */
/* snake                                                              */
/* ================================================================== */

#define SNAKE_MAX 512
#define SNAKE_LEFT   1
#define SNAKE_RIGHT  78
#define SNAKE_TOP    2
#define SNAKE_BOTTOM 22

static int snake_x[SNAKE_MAX];
static int snake_y[SNAKE_MAX];
static int snake_len;
static int snake_dx, snake_dy;
static int food_x, food_y;
static uint32_t snake_rng;
static uint32_t snake_score;
static int snake_speed_ms;

static uint32_t rng_next(void)
{
    snake_rng = snake_rng * 1103515245u + 12345u;
    return snake_rng;
}

static int snake_contains(int x, int y, int count)
{
    for (int i = 0; i < count; i++) {
        if (snake_x[i] == x && snake_y[i] == y) return 1;
    }
    return 0;
}

static void snake_place_food(void)
{
    for (int tries = 0; tries < 500; tries++) {
        int x = SNAKE_LEFT + (int)(rng_next() % (SNAKE_RIGHT - SNAKE_LEFT + 1));
        int y = SNAKE_TOP + (int)(rng_next() % (SNAKE_BOTTOM - SNAKE_TOP + 1));
        if (!snake_contains(x, y, snake_len)) {
            food_x = x;
            food_y = y;
            return;
        }
    }
    food_x = SNAKE_LEFT;
    food_y = SNAKE_TOP;
}

static void snake_draw_border(void)
{
    console_set_color(COLOR_CYAN);
    for (int x = 0; x < 80; x++) {
        console_gotoxy((uint32_t)x, 1);
        console_putc((x == 0 || x == 79) ? '+' : '-');
        console_gotoxy((uint32_t)x, 23);
        console_putc((x == 0 || x == 79) ? '+' : '-');
    }
    for (int y = 2; y <= 22; y++) {
        console_gotoxy(0, (uint32_t)y);
        console_putc('|');
        console_gotoxy(79, (uint32_t)y);
        console_putc('|');
    }
    console_set_color(COLOR_TEXT);
}

static void snake_header(void)
{
    console_gotoxy(0, 0);
    console_set_color(COLOR_TITLE);
    console_printf("SNAKE   Score: %-4u Length: %-3d  [arrows/WASD]  [P]ause  [ESC]quit   ",
                   snake_score, snake_len);
    console_set_color(COLOR_TEXT);
}

static void snake_draw_all(void)
{
    console_clear();
    snake_draw_border();

    console_gotoxy((uint32_t)food_x, (uint32_t)food_y);
    console_set_color(COLOR_LIGHT_RED);
    console_putc('*');

    for (int i = 0; i < snake_len; i++) {
        console_gotoxy((uint32_t)snake_x[i], (uint32_t)snake_y[i]);
        console_set_color(i == snake_len - 1 ? COLOR_LIGHT_GREEN : COLOR_GREEN);
        console_putc(i == snake_len - 1 ? '@' : 'o');
    }
    console_set_color(COLOR_TEXT);
    snake_header();
}

static void snake_reset(void)
{
    snake_len = 4;
    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = 40 - (snake_len - 1 - i);
        snake_y[i] = 12;
    }
    snake_dx = 1;
    snake_dy = 0;
    snake_score = 0;
    snake_speed_ms = 150;
    snake_rng = (uint32_t)rdtsc() ^ timer_ticks() ^ 0x5EED1234u;
    if (snake_rng == 0) snake_rng = 1;
    snake_place_food();
    snake_draw_all();
}

static int snake_game_over(void)
{
    console_set_color(COLOR_LIGHT_RED);
    console_gotoxy(32, 10);
    console_write("GAME OVER");
    console_gotoxy(30, 12);
    console_printf("Score: %u", snake_score);
    console_gotoxy(22, 14);
    console_set_color(COLOR_TEXT);
    console_write("ENTER = play again      ESC = quit");

    for (;;) {
        int k = input_wait();
        if (k == KEY_ENTER) return 1;
        if (k == KEY_ESC || k == 'q' || k == 'Q') return 0;
    }
}

static void prog_snake(void)
{
    for (;;) {
        snake_reset();
        int alive = 1;
        int paused = 0;
        int pending = 0;
        int pdx = 0, pdy = 0;
        uint32_t next_move = timer_ticks() + (uint32_t)snake_speed_ms / 10u;

        while (alive) {
            int key = input_poll();
            while (key) {
                if (key == KEY_ESC || key == 'q' || key == 'Q') {
                    console_clear();
                    console_write_line("Snake closed.");
                    return;
                }
                if (key == 'p' || key == 'P') {
                    paused = !paused;
                    console_gotoxy(60, 0);
                    console_set_color(paused ? COLOR_WARN : COLOR_TEXT);
                    console_printf("%-16s", paused ? "** PAUSED **" : "");
                    console_set_color(COLOR_TEXT);
                }
                if (!paused) {
                    if (key == KEY_UP || key == 'w' || key == 'W') { pdx = 0; pdy = -1; pending = 1; }
                    else if (key == KEY_DOWN || key == 's' || key == 'S') { pdx = 0; pdy = 1; pending = 1; }
                    else if (key == KEY_LEFT || key == 'a' || key == 'A') { pdx = -1; pdy = 0; pending = 1; }
                    else if (key == KEY_RIGHT || key == 'd' || key == 'D') { pdx = 1; pdy = 0; pending = 1; }
                }
                key = input_poll();
            }

            if (paused) {
                cpu_idle();
                continue;
            }

            if ((int32_t)(timer_ticks() - next_move) >= 0) {
                next_move = timer_ticks() + (uint32_t)snake_speed_ms / 10u;

                if (pending) {
                    if (!(pdx == -snake_dx && pdy == -snake_dy)) {
                        snake_dx = pdx;
                        snake_dy = pdy;
                    }
                    pending = 0;
                }

                int hx = snake_x[snake_len - 1] + snake_dx;
                int hy = snake_y[snake_len - 1] + snake_dy;

                if (hx < SNAKE_LEFT || hx > SNAKE_RIGHT ||
                    hy < SNAKE_TOP || hy > SNAKE_BOTTOM) {
                    alive = 0;
                    break;
                }

                int grow = (hx == food_x && hy == food_y);
                if (snake_contains(hx, hy, grow ? snake_len : snake_len - 1)) {
                    alive = 0;
                    break;
                }

                int old_tail_x = snake_x[0];
                int old_tail_y = snake_y[0];
                int old_head_x = snake_x[snake_len - 1];
                int old_head_y = snake_y[snake_len - 1];

                if (grow) {
                    if (snake_len < SNAKE_MAX) {
                        snake_len++;
                    }
                } else {
                    for (int i = 0; i < snake_len - 1; i++) {
                        snake_x[i] = snake_x[i + 1];
                        snake_y[i] = snake_y[i + 1];
                    }
                }
                snake_x[snake_len - 1] = hx;
                snake_y[snake_len - 1] = hy;

                if (!grow) {
                    console_gotoxy((uint32_t)old_tail_x, (uint32_t)old_tail_y);
                    console_putc(' ');
                }

                console_gotoxy((uint32_t)old_head_x, (uint32_t)old_head_y);
                console_set_color(COLOR_GREEN);
                console_putc('o');

                console_gotoxy((uint32_t)hx, (uint32_t)hy);
                console_set_color(COLOR_LIGHT_GREEN);
                console_putc('@');
                console_set_color(COLOR_TEXT);

                if (grow) {
                    snake_score += 10;
                    if (snake_speed_ms > 60) snake_speed_ms -= 4;
                    snake_place_food();
                    console_gotoxy((uint32_t)food_x, (uint32_t)food_y);
                    console_set_color(COLOR_LIGHT_RED);
                    console_putc('*');
                    console_set_color(COLOR_TEXT);
                }

                snake_header();
            }

            cpu_idle();
        }

        if (!snake_game_over()) {
            console_clear();
            console_write_line("Snake closed.");
            return;
        }
    }
}

/* ================================================================== */
/* task manager                                                       */
/* ================================================================== */

static void tm_sep(int y)
{
    console_gotoxy(0, (uint32_t)y);
    console_putc('+');
    for (int i = 0; i < 78; i++) console_putc('-');
    console_putc('+');
}

static void tm_title(int y, const char *title)
{
    console_gotoxy(0, (uint32_t)y);
    console_putc('+');
    console_putc('-');
    console_putc(' ');
    console_write(title);
    console_putc(' ');
    int used = 3 + (int)strlen(title) + 1;
    while (used < 79) { console_putc('-'); used++; }
    console_putc('+');
}

static void tm_content_line(int y)
{
    console_gotoxy(0, (uint32_t)y);
    console_putc('|');
    for (int i = 0; i < 78; i++) console_putc(' ');
    console_putc('|');
}

static void put_field(int x, int y, int width, const char *text)
{
    if (width <= 0) return;
    console_gotoxy((uint32_t)x, (uint32_t)y);
    int n = (int)strlen(text);
    if (n > width) n = width;
    for (int i = 0; i < n; i++) console_putc(text[i]);
    for (int i = n; i < width; i++) console_putc(' ');
}

static void put_num(int x, int y, int width, uint32_t value)
{
    char buf[16];
    utoa(value, buf, 10);
    put_field(x, y, width, buf);
}

static void put_bar(int x, int y, uint32_t percent)
{
    if (percent > 100) percent = 100;
    int filled = (int)(percent / 10);
    console_set_color(percent >= 85 ? COLOR_LIGHT_RED :
                      percent >= 60 ? COLOR_YELLOW : COLOR_LIGHT_GREEN);
    console_gotoxy((uint32_t)x, (uint32_t)y);
    for (int i = 0; i < 10; i++) console_putc(i < filled ? '#' : '-');
    console_set_color(COLOR_TEXT);
}

static void tm_static(void)
{
    console_clear();
    tm_title(0, "DEBIL TASK MANAGER");
    for (int y = 1; y <= 8; y++) tm_content_line(y);
    tm_sep(9);
    for (int y = 10; y <= 13; y++) tm_content_line(y);
    tm_sep(14);
    for (int y = 15; y <= 17; y++) tm_content_line(y);
    tm_sep(18);
    tm_content_line(19);
    tm_sep(20);

    console_gotoxy(2, 1);  console_write("Uptime:");
    console_gotoxy(24, 1); console_write("Ticks:");
    console_gotoxy(46, 1); console_write("PIT: 100 Hz");

    console_set_color(COLOR_TITLE);
    console_gotoxy(2, 2);  console_write("CPU:");
    console_set_color(COLOR_TEXT);
    console_gotoxy(2, 3);  console_write("Cores:");
    console_gotoxy(14, 3); console_write("Clock: ~");
    console_gotoxy(30, 3); console_write("MHz");

    console_gotoxy(2, 4);  console_write("CPU usage:");
    console_gotoxy(13, 4); console_write("[----------]");
    console_gotoxy(27, 4); console_write("%");
    console_gotoxy(30, 4); console_write("(measured busy vs HLT)");

    console_gotoxy(2, 5);  console_write("RAM:");
    console_gotoxy(12, 5); console_write("MB total");
    console_gotoxy(22, 5); console_write("Used:");
    console_gotoxy(28, 5); console_write("[----------]");
    console_gotoxy(43, 5); console_write("%");
    console_gotoxy(45, 5); console_write("Kernel:");
    console_gotoxy(60, 5); console_write("KB");

    console_set_color(COLOR_TITLE);
    console_gotoxy(2, 6);  console_write("GPU:");
    console_set_color(COLOR_TEXT);
    console_gotoxy(2, 7);  console_write("VRAM:");
    console_gotoxy(14, 7); console_write("KB");
    console_gotoxy(24, 7); console_write("GPU text activity:");
    console_gotoxy(43, 7); console_write("[----------]");
    console_gotoxy(58, 7); console_write("%");

    console_gotoxy(2, 8);  console_write("FS:");
    console_gotoxy(9, 8);  console_write("files /");
    console_gotoxy(25, 8); console_write("bytes");
    console_gotoxy(38, 8); console_write("Console chars/s:");

    console_set_color(COLOR_TITLE);
    console_gotoxy(2, 10); console_write("PID  TASK              STATE        CPU          MEM");
    console_set_color(COLOR_TEXT);
    console_gotoxy(2, 11); console_write("1    kernel            running      measured     footprint");
    console_gotoxy(2, 12); console_write("2    shell             waiting      idle         -");
    console_gotoxy(2, 13); console_write("3    tasks.debil       running      one core     -");

    console_gotoxy(2, 15); console_write("Note: single-core cooperative kernel, no preemption.");
    console_gotoxy(2, 16); console_write("CPU% = busy PIT ticks / (busy + HLT ticks).");
    console_gotoxy(2, 17); console_write("GPU% = VGA text activity proxy, not 3D load.");

    console_set_color(COLOR_PROMPT);
    console_gotoxy(2, 19); console_write("q / ESC = exit");
    console_set_color(COLOR_TEXT);
}

static void prog_tasks(void)
{
    g_current_program = "tasks.debil";
    tm_static();

    for (;;) {
        int key = input_poll();
        if (key == KEY_ESC || key == 'q' || key == 'Q') break;

        uint32_t sec = timer_uptime_seconds();
        char up[16];
        char h[8], m[8], s[8];
        utoa(sec / 3600u, h, 10);
        utoa((sec / 60u) % 60u, m, 10);
        utoa(sec % 60u, s, 10);
        int p = 0;
        if (strlen(h) < 2) up[p++] = '0';
        for (int i = 0; h[i]; i++) up[p++] = h[i];
        up[p++] = ':';
        if (strlen(m) < 2) up[p++] = '0';
        for (int i = 0; m[i]; i++) up[p++] = m[i];
        up[p++] = ':';
        if (strlen(s) < 2) up[p++] = '0';
        for (int i = 0; s[i]; i++) up[p++] = s[i];
        up[p] = 0;
        put_field(10, 1, 10, up);
        put_num(31, 1, 10, timer_ticks());

        put_field(7, 2, 70, cpu_brand());
        put_num(9, 3, 3, cpu_cores());
        put_num(23, 3, 6, cpu_mhz());

        uint32_t cpu = timer_cpu_percent();
        put_bar(13, 4, cpu);
        put_num(24, 4, 3, cpu);

        uint32_t total_kb = mem_total_kb();
        uint32_t used_kb = (mem_kernel_bytes() / 1024u) + (fs_used_bytes() / 1024u);
        uint32_t ram_pct = total_kb ? (used_kb * 100u) / total_kb : 0;
        if (ram_pct > 100) ram_pct = 100;
        put_num(7, 5, 4, total_kb / 1024u);
        put_bar(28, 5, ram_pct);
        put_num(39, 5, 3, ram_pct);
        put_num(53, 5, 6, mem_kernel_bytes() / 1024u);

        if (pci_gpu_count() > 0) {
            const pci_gpu_t *g = pci_gpu_get(0);
            char gpu[80];
            char id[16];
            utoa(g->vendor, id, 16);
            char id2[16];
            utoa(g->device_id, id2, 16);
            strcpy(gpu, pci_vendor_name(g->vendor));
            strcat(gpu, "  ");
            strcat(gpu, id);
            strcat(gpu, ":");
            strcat(gpu, id2);
            put_field(7, 6, 70, gpu);

            uint32_t vram_kb = (g->bar0_size ? g->bar0_size : g->bar2_size) / 1024u;
            put_num(8, 7, 6, vram_kb);
        } else {
            put_field(7, 6, 70, "no PCI display controller detected");
            put_num(8, 7, 6, 0);
        }

        put_num(6, 8, 3, fs_count());
        put_num(18, 8, 6, fs_used_bytes());

        uint32_t cps = timer_chars_per_sec();
        uint32_t gpu_pct = cps / 200u;
        if (gpu_pct > 100) gpu_pct = 100;
        put_bar(43, 7, gpu_pct);
        put_num(54, 7, 3, gpu_pct);
        put_num(55, 8, 8, cps);

        timer_sleep_ms(250);
    }

    console_clear();
    console_set_color(COLOR_OK);
    console_write_line("Task manager closed.");
    console_set_color(COLOR_TEXT);
    g_current_program = "none";
}

/* ================================================================== */
/* crash tester                                                       */
/* ================================================================== */

static void prog_crash(void)
{
    for (;;) {
        console_clear();
        console_set_color(COLOR_ERROR);
        console_write_line("==================================================================");
        console_write_line("                     D E B I L   C R A S H E R");
        console_write_line("==================================================================");
        console_set_color(COLOR_TEXT);
        console_write_line("");
        console_write_line("This program intentionally faults the kernel, then resets the");
        console_write_line("machine after showing the exception dump. It is a reboot test.");
        console_write_line("");
        console_write_line("  1) CPU fault   - invalid opcode (ud2)");
        console_write_line("  2) RAM fault   - page fault on unmapped address 0x40000000");
        console_write_line("  3) GPU fault   - page fault on GPU MMIO range 0xE0000000");
        console_write_line("  Q) cancel and return to the shell");
        console_write_line("");
        console_write_line("WARNING: anything not saved in the in-memory filesystem is lost.");

        char line[32];
        int n = shell_read_line("select> ", line, sizeof(line));
        if (n < 0) continue;
        str_trim(line);

        if (strcasecmp(line, "q") == 0) {
            console_clear();
            console_write_line("Crash test cancelled.");
            return;
        }
        if (strcmp(line, "1") == 0 || strcasecmp(line, "cpu") == 0) crash_trigger(CRASH_CPU);
        if (strcmp(line, "2") == 0 || strcasecmp(line, "ram") == 0) crash_trigger(CRASH_RAM);
        if (strcmp(line, "3") == 0 || strcasecmp(line, "gpu") == 0) crash_trigger(CRASH_GPU);

        console_set_color(COLOR_WARN);
        console_write_line("Invalid choice.");
        console_set_color(COLOR_TEXT);
        timer_sleep_ms(600);
    }
}

/* ================================================================== */
/* calculator                                                         */
/* ================================================================== */

static void prog_calc(void)
{
    console_clear();
    console_set_color(COLOR_TITLE);
    console_write_line("DEBIL CALC - integer arithmetic");
    console_set_color(COLOR_TEXT);
    console_write_line("Enter expressions like: 12 + 7   or   100 / 4");
    console_write_line("Type q to quit.");
    console_write_line("");

    char line[SHELL_MAX_LINE];
    for (;;) {
        int n = shell_read_line("calc> ", line, SHELL_MAX_LINE);
        if (n < 0) continue;
        str_trim(line);
        if (!line[0]) continue;
        if (strcasecmp(line, "q") == 0 || strcasecmp(line, "quit") == 0) break;

        char *op = 0;
        for (char *p = line; *p; p++) {
            if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%') {
                op = p;
                break;
            }
        }
        if (!op) {
            console_write_line("calc: expected: <number> <op> <number>");
            continue;
        }

        char opchar = *op;
        *op = 0;
        int a = atoi(line);
        int b = atoi(op + 1);
        int32_t result = 0;
        int ok = 1;

        switch (opchar) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/':
            if (b == 0) { console_write_line("calc: division by zero"); ok = 0; }
            else result = a / b;
            break;
        case '%':
            if (b == 0) { console_write_line("calc: division by zero"); ok = 0; }
            else result = a % b;
            break;
        }

        if (ok) console_printf("%d %c %d = %d\n", a, opchar, b, result);
    }

    console_clear();
    console_write_line("Calculator closed.");
}
