/* DEBIL OS - terminal shell */
#include "shell.h"
#include "console.h"
#include "input.h"
#include "string.h"
#include "timer.h"
#include "fs.h"
#include "cpu.h"
#include "mem.h"
#include "pci.h"
#include "programs.h"
#include "panic.h"
#include "io.h"

#define HISTORY_MAX 16
#define HISTORY_LEN SHELL_MAX_LINE

static char g_history[HISTORY_MAX][HISTORY_LEN];
static int  g_history_count = 0;

static const char *g_version = "1.0";

/* ------------------------------------------------------------------ */
/* Line editor                                                        */
/* ------------------------------------------------------------------ */

static void history_add(const char *line)
{
    if (!line[0]) return;
    if (g_history_count > 0 &&
        strcmp(g_history[g_history_count - 1], line) == 0) {
        return;
    }
    if (g_history_count == HISTORY_MAX) {
        memmove(g_history[0], g_history[1], (HISTORY_MAX - 1) * HISTORY_LEN);
        g_history_count--;
    }
    strncpy(g_history[g_history_count], line, HISTORY_LEN - 1);
    g_history[g_history_count][HISTORY_LEN - 1] = 0;
    g_history_count++;
}

static void redraw_input(const char *prompt, const char *buf)
{
    uint32_t y = console_get_y();
    console_gotoxy(0, y);
    console_set_color(COLOR_PROMPT);
    console_write(prompt);
    console_set_color(COLOR_TEXT);
    console_write(buf);

    int len = (int)strlen(buf);
    for (int i = len; i < 64; i++) console_putc(' ');
    console_gotoxy((uint32_t)(strlen(prompt) + (size_t)len), y);
}

static int read_line_ex(const char *prompt, char *buf, int max, int use_history)
{
    int len = 0;
    int hist_pos = g_history_count;

    buf[0] = 0;
    console_set_color(COLOR_PROMPT);
    console_write(prompt);
    console_set_color(COLOR_TEXT);

    for (;;) {
        int key = input_wait();

        if (key == KEY_ENTER) {
            console_putc('\n');
            buf[len] = 0;
            return len;
        }
        if (key == KEY_CTRLC) {
            console_write("^C\n");
            buf[0] = 0;
            return -1;
        }
        if (key == KEY_ESC) {
            len = 0;
            buf[0] = 0;
            redraw_input(prompt, buf);
            continue;
        }
        if (key == KEY_BACKSPACE) {
            if (len > 0) {
                len--;
                console_putc('\b');
            }
            continue;
        }
        if (key == KEY_UP && use_history) {
            if (hist_pos > 0) {
                hist_pos--;
                strncpy(buf, g_history[hist_pos], (size_t)max - 1);
                buf[max - 1] = 0;
                len = (int)strlen(buf);
                redraw_input(prompt, buf);
            }
            continue;
        }
        if (key == KEY_DOWN && use_history) {
            if (hist_pos < g_history_count - 1) {
                hist_pos++;
                strncpy(buf, g_history[hist_pos], (size_t)max - 1);
                buf[max - 1] = 0;
            } else {
                hist_pos = g_history_count;
                buf[0] = 0;
            }
            len = (int)strlen(buf);
            redraw_input(prompt, buf);
            continue;
        }
        if (key >= 32 && key < 127 && len < max - 1) {
            buf[len++] = (char)key;
            console_putc((char)key);
        }
    }
}

int shell_read_line(const char *prompt, char *buf, int max)
{
    return read_line_ex(prompt, buf, max, 0);
}

void shell_print_prompt(void)
{
    console_set_color(COLOR_PROMPT);
    console_write("debil> ");
    console_set_color(COLOR_TEXT);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static int split_args(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') *p++ = 0;
        if (!*p) break;
        if (argc >= max) break;

        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = 0;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = 0;
        }
    }
    return argc;
}

static void join_from(char *dst, char **argv, int argc, int start)
{
    dst[0] = 0;
    for (int i = start; i < argc; i++) {
        if (i > start) strcat(dst, " ");
        strcat(dst, argv[i]);
    }
}

static void print_size(uint32_t bytes)
{
    if (bytes >= 1024u * 1024u) {
        console_printf("%u.%u MB", bytes / (1024u * 1024u),
                       ((bytes / 1024u) % 1024u) * 10u / 1024u);
    } else if (bytes >= 1024u) {
        console_printf("%u.%u KB", bytes / 1024u, ((bytes % 1024u) * 10u) / 1024u);
    } else {
        console_printf("%u B", bytes);
    }
}

static void print_uptime(void)
{
    uint32_t sec = timer_uptime_seconds();
    uint32_t h = sec / 3600u;
    uint32_t m = (sec / 60u) % 60u;
    uint32_t s = sec % 60u;
    console_printf("%02u:%02u:%02u", h, m, s);
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

static uint32_t bcd_to_bin(uint8_t v)
{
    return (uint32_t)(v & 0x0F) + ((uint32_t)(v >> 4) * 10u);
}

static void cmd_date(void)
{
    cpu_cli();
    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t status_b = cmos_read(0x0B);
    cpu_sti();

    if (!(status_b & 0x04)) {
        sec = (uint8_t)bcd_to_bin(sec);
        min = (uint8_t)bcd_to_bin(min);
        hour = (uint8_t)bcd_to_bin(hour);
        day = (uint8_t)bcd_to_bin(day);
        month = (uint8_t)bcd_to_bin(month);
        year = (uint8_t)bcd_to_bin(year);
    }
    if (!(status_b & 0x02) && (hour & 0x80)) {
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    }

    console_printf("RTC: 20%02u-%02u-%02u %02u:%02u:%02u\n",
                   year, month, day, hour, min, sec);
    console_write_line("(CMOS local time, no timezone applied)");
}

static void cmd_help(void)
{
    console_set_color(COLOR_TITLE);
    console_write_line("DEBIL OS commands");
    console_set_color(COLOR_TEXT);
    console_write_line("  help                 this list");
    console_write_line("  clear                clear the screen");
    console_write_line("  echo <text>          print text");
    console_write_line("  ls                   list files");
    console_write_line("  mkfile|touch <name>  create an empty file");
    console_write_line("  edit <name>          line editor (:w save, :q quit, :wq)");
    console_write_line("  cat <name>           print a file");
    console_write_line("  write <name> <text>  append text to a file (creates it)");
    console_write_line("  rm <name>            delete a file");
    console_write_line("  cp <src> <dst>       copy a file");
    console_write_line("  mv <src> <dst>       rename a file");
    console_write_line("  df                   filesystem usage");
    console_write_line("  stat <name>          file details");
    console_write_line("");
    console_write_line("  programs             list installed .debil programs");
    console_write_line("  start <name.debil>   run a program");
    console_write_line("  tasks                live task manager");
    console_write_line("");
    console_write_line("  sysinfo              CPU / RAM / GPU summary");
    console_write_line("  meminfo              E820 memory map");
    console_write_line("  pci                  PCI display devices");
    console_write_line("  uptime               time since boot");
    console_write_line("  date                 read the CMOS clock");
    console_write_line("  stress [ms]          burn CPU to test the meter");
    console_write_line("  history              command history");
    console_write_line("");
    console_write_line("  test_crash <cpu|ram|gpu>   intentional fault + reboot");
    console_write_line("  reboot               reset the machine");
    console_write_line("  shutdown             power off (ACPI)");
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static void cmd_ls(void)
{
    console_set_color(COLOR_TITLE);
    console_printf("%-24s %8s\n", "NAME", "SIZE");
    console_set_color(COLOR_TEXT);

    uint32_t n = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_get(i);
        if (!f) continue;
        console_printf("%-24s %8u\n", f->name, f->size);
        n++;
    }
    if (n == 0) console_write_line("(no files)");

    console_printf("\n%u file(s), ", n);
    print_size(fs_used_bytes());
    console_write(" used of ");
    print_size(fs_capacity_bytes());
    console_write_line("");
}

static void cmd_edit(const char *name)
{
    fs_file_t *f = fs_find(name);
    if (f && f->size >= FS_MAX_DATA - 1) {
        console_set_color(COLOR_ERROR);
        console_write_line("File is full (4 KiB limit).");
        console_set_color(COLOR_TEXT);
        return;
    }

    char buffer[FS_MAX_DATA];
    uint32_t used = 0;
    int dirty = 0;

    memset(buffer, 0, sizeof(buffer));
    if (f) {
        memcpy(buffer, f->data, f->size);
        used = f->size;
    }

    console_clear();
    console_set_color(COLOR_TITLE);
    console_printf("DEBIL EDITOR - %s\n", name);
    console_set_color(COLOR_TEXT);
    console_write_line("Type lines and press Enter. Commands:");
    console_write_line("  :w   save     :q   quit     :q!  discard     :wq  save+quit");
    console_write_line("--------------------------------------------------------------");

    if (f) {
        console_write_line("--- existing content ---");
        for (uint32_t i = 0; i < f->size; i++) console_putc((char)f->data[i]);
        if (f->size && f->data[f->size - 1] != '\n') console_putc('\n');
        console_write_line("------------------------");
    }

    char line[SHELL_MAX_LINE];
    for (;;) {
        int n = shell_read_line("> ", line, SHELL_MAX_LINE);
        if (n < 0) continue;
        str_trim(line);

        if (strcmp(line, ":q") == 0 || strcmp(line, ":quit") == 0) {
            if (dirty) {
                console_set_color(COLOR_WARN);
                console_write_line("Unsaved changes. Use :wq to save or :q! to discard.");
                console_set_color(COLOR_TEXT);
                continue;
            }
            break;
        }
        if (strcmp(line, ":q!") == 0) {
            console_write_line("Changes discarded.");
            break;
        }
        if (strcmp(line, ":w") == 0 || strcmp(line, ":wq") == 0) {
            fs_file_t *target = fs_find(name);
            if (!target) target = fs_create(name);
            if (!target) {
                console_set_color(COLOR_ERROR);
                console_write_line("Could not create file (disk full or bad name).");
                console_set_color(COLOR_TEXT);
                continue;
            }
            fs_write(name, buffer, used);
            console_set_color(COLOR_OK);
            console_printf("Saved %u bytes to %s.\n", used, name);
            console_set_color(COLOR_TEXT);
            dirty = 0;
            if (strcmp(line, ":wq") == 0) break;
            continue;
        }

        uint32_t add = (uint32_t)strlen(line);
        if (used + add + 1 >= FS_MAX_DATA - 1) {
            console_set_color(COLOR_ERROR);
            console_write_line("Buffer full (4 KiB limit). Save with :wq.");
            console_set_color(COLOR_TEXT);
            continue;
        }
        memcpy(buffer + used, line, add);
        used += add;
        buffer[used++] = '\n';
        buffer[used] = 0;
        dirty = 1;
    }

    console_clear();
    console_set_color(COLOR_OK);
    console_printf("Editor closed: %s\n", name);
    console_set_color(COLOR_TEXT);
}

static void cmd_cat(const char *name)
{
    fs_file_t *f = fs_find(name);
    if (!f) {
        console_set_color(COLOR_ERROR);
        console_printf("cat: %s: no such file\n", name);
        console_set_color(COLOR_TEXT);
        return;
    }
    for (uint32_t i = 0; i < f->size; i++) console_putc((char)f->data[i]);
    if (f->size == 0 || f->data[f->size - 1] != '\n') console_putc('\n');
}

static void cmd_write(const char *name, const char *text)
{
    if (!fs_valid_name(name)) {
        console_set_color(COLOR_ERROR);
        console_write_line("write: invalid file name");
        console_set_color(COLOR_TEXT);
        return;
    }
    if (!fs_find(name) && !fs_create(name)) {
        console_set_color(COLOR_ERROR);
        console_write_line("write: could not create file");
        console_set_color(COLOR_TEXT);
        return;
    }
    fs_append(name, text, (uint32_t)strlen(text));
    fs_append(name, "\n", 1);
    fs_file_t *f = fs_find(name);
    console_set_color(COLOR_OK);
    console_printf("Wrote %u bytes to %s (%u bytes total).\n",
                   (uint32_t)strlen(text) + 1, name, f ? f->size : 0);
    console_set_color(COLOR_TEXT);
}

static void cmd_stat(const char *name)
{
    fs_file_t *f = fs_find(name);
    if (!f) {
        console_set_color(COLOR_ERROR);
        console_printf("stat: %s: no such file\n", name);
        console_set_color(COLOR_TEXT);
        return;
    }
    console_printf("File : %s\n", f->name);
    console_printf("Size : %u bytes (%u / %u used)\n", f->size, f->size, FS_MAX_DATA - 1);
    console_printf("Data : %u%% of capacity\n", (f->size * 100u) / (FS_MAX_DATA - 1));
}

static void cmd_df(void)
{
    console_printf("Files     : %u / %u\n", fs_count(), FS_MAX_FILES);
    console_printf("Data used : %u bytes\n", fs_used_bytes());
    console_printf("Capacity  : %u bytes\n", fs_capacity_bytes());
    console_printf("Free      : %u bytes\n", fs_capacity_bytes() - fs_used_bytes());
    console_printf("RAM total : %u MB usable (E820)\n", mem_total_kb() / 1024u);
}

static void cmd_sysinfo(void)
{
    console_set_color(COLOR_TITLE);
    console_write_line("DEBIL OS system information");
    console_set_color(COLOR_TEXT);
    console_printf("OS        : DEBIL OS v%s, 32-bit x86, terminal\n", g_version);
    console_printf("Built     : %s %s\n", __DATE__, __TIME__);
    console_printf("Uptime    : ");
    print_uptime();
    console_putc('\n');
    console_write_line("");
    console_printf("CPU       : %s\n", cpu_brand());
    console_printf("Vendor    : %s\n", cpu_vendor());
    console_printf("Cores     : %u    Measured clock: ~%u MHz\n", cpu_cores(), cpu_mhz());
    console_printf("Features  : FPU=%u TSC=%u SSE=%u SSE2=%u HTT=%u\n",
                   (cpu_features_edx() >> 0) & 1,
                   (cpu_features_edx() >> 4) & 1,
                   (cpu_features_edx() >> 25) & 1,
                   (cpu_features_edx() >> 26) & 1,
                   (cpu_features_edx() >> 28) & 1);
    console_write_line("");
    console_printf("RAM       : %u MB usable   Kernel footprint: %u KB\n",
                   mem_total_kb() / 1024u, mem_kernel_bytes() / 1024u);
    console_printf("Identity  : 0 - 256 MB mapped, 4 MB pages, CR0.PG=1\n");
    console_printf("Kernel end: 0x%x\n", mem_kernel_end());
    console_write_line("");

    if (pci_gpu_count() > 0) {
        const pci_gpu_t *g = pci_gpu_get(0);
        console_printf("GPU       : %s %04x:%04x (bus %u dev %u)\n",
                       pci_vendor_name(g->vendor), g->vendor, g->device_id,
                       g->bus, g->device);
        console_printf("VRAM BAR0 : 0x%x (%u KB)   class: %s\n",
                       g->bar0, g->bar0_size / 1024u,
                       pci_class_name(g->class_code, g->subclass));
    } else {
        console_write_line("GPU       : no PCI display controller found");
    }
    console_printf("PCI       : %u device(s), %u display controller(s)\n",
                   pci_device_count(), pci_gpu_count());
    console_write_line("");
    console_printf("Filesystem: %u file(s), %u bytes used\n", fs_count(), fs_used_bytes());
}

static void cmd_stress(const char *arg)
{
    uint32_t ms = arg ? (uint32_t)atoi(arg) : 3000;
    if (ms == 0) ms = 3000;
    uint32_t end = timer_ticks() + ms / 10u;
    console_printf("Burning CPU for %u ms (watch CPU usage in 'tasks')...\n", ms);
    uint32_t x = 1;
    while ((int32_t)(timer_ticks() - end) < 0) {
        for (volatile int i = 0; i < 10000; i++) {
            x = x * 1664525u + 1013904223u;
        }
    }
    console_printf("Done (spinner=%x).\n", x);
}

static void cmd_pci(void)
{
    console_printf("PCI devices: %u, display controllers: %u\n",
                   pci_device_count(), pci_gpu_count());
    if (pci_gpu_count() == 0) {
        console_write_line("No display controller found.");
        return;
    }
    for (uint32_t i = 0; i < pci_gpu_count(); i++) {
        const pci_gpu_t *g = pci_gpu_get(i);
        console_printf("\nGPU %u: %s\n", i, pci_vendor_name(g->vendor));
        console_printf("  ID       : %04x:%04x\n", g->vendor, g->device_id);
        console_printf("  Location : PCI bus %u, device %u, function %u\n",
                       g->bus, g->device, g->function);
        console_printf("  Class    : %s (0x%02x/0x%02x)\n",
                       pci_class_name(g->class_code, g->subclass),
                       g->class_code, g->subclass);
        console_printf("  BAR0     : 0x%x  size %u KB\n", g->bar0, g->bar0_size / 1024u);
        console_printf("  BAR2     : 0x%x  size %u KB\n", g->bar2, g->bar2_size / 1024u);
    }
}

static void cmd_test_crash(const char *mode)
{
    if (!mode) {
        console_set_color(COLOR_WARN);
        console_write_line("Usage: test_crash <cpu|ram|gpu>");
        console_write_line("WARNING: this intentionally faults the kernel and resets the machine.");
        console_set_color(COLOR_TEXT);
        return;
    }
    if (strcasecmp(mode, "cpu") == 0) crash_trigger(CRASH_CPU);
    else if (strcasecmp(mode, "ram") == 0) crash_trigger(CRASH_RAM);
    else if (strcasecmp(mode, "gpu") == 0) crash_trigger(CRASH_GPU);
    else {
        console_set_color(COLOR_ERROR);
        console_write_line("test_crash: mode must be cpu, ram or gpu");
        console_set_color(COLOR_TEXT);
    }
}

void shell_execute(char *line)
{
    str_trim(line);
    if (!line[0]) return;

    char raw[SHELL_MAX_LINE];
    strncpy(raw, line, SHELL_MAX_LINE - 1);
    raw[SHELL_MAX_LINE - 1] = 0;

    char *argv[12];
    int argc = split_args(line, argv, 12);
    if (argc == 0) return;

    const char *cmd = argv[0];

    if (strcasecmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        cmd_help();
    } else if (strcasecmp(cmd, "clear") == 0 || strcasecmp(cmd, "cls") == 0) {
        console_clear();
    } else if (strcasecmp(cmd, "echo") == 0) {
        char text[SHELL_MAX_LINE];
        join_from(text, argv, argc, 1);
        console_write_line(text);
    } else if (strcasecmp(cmd, "ls") == 0 || strcasecmp(cmd, "dir") == 0) {
        cmd_ls();
    } else if (strcasecmp(cmd, "mkfile") == 0 || strcasecmp(cmd, "touch") == 0 ||
               strcasecmp(cmd, "create") == 0) {
        if (argc < 2) {
            console_write_line("usage: mkfile <name>");
        } else if (!fs_valid_name(argv[1])) {
            console_set_color(COLOR_ERROR);
            console_write_line("mkfile: invalid file name");
            console_set_color(COLOR_TEXT);
        } else if (fs_find(argv[1])) {
            console_printf("mkfile: %s already exists\n", argv[1]);
        } else if (!fs_create(argv[1])) {
            console_set_color(COLOR_ERROR);
            console_write_line("mkfile: filesystem full");
            console_set_color(COLOR_TEXT);
        } else {
            console_set_color(COLOR_OK);
            console_printf("Created %s\n", argv[1]);
            console_set_color(COLOR_TEXT);
        }
    } else if (strcasecmp(cmd, "edit") == 0) {
        if (argc < 2) console_write_line("usage: edit <name>");
        else cmd_edit(argv[1]);
    } else if (strcasecmp(cmd, "cat") == 0 || strcasecmp(cmd, "type") == 0) {
        if (argc < 2) console_write_line("usage: cat <name>");
        else cmd_cat(argv[1]);
    } else if (strcasecmp(cmd, "write") == 0 || strcasecmp(cmd, "append") == 0) {
        if (argc < 3) {
            console_write_line("usage: write <name> <text>");
        } else {
            char text[SHELL_MAX_LINE];
            join_from(text, argv, argc, 2);
            cmd_write(argv[1], text);
        }
    } else if (strcasecmp(cmd, "rm") == 0 || strcasecmp(cmd, "del") == 0) {
        if (argc < 2) console_write_line("usage: rm <name>");
        else if (!fs_delete(argv[1])) {
            console_set_color(COLOR_ERROR);
            console_printf("rm: %s: no such file\n", argv[1]);
            console_set_color(COLOR_TEXT);
        } else {
            console_set_color(COLOR_OK);
            console_printf("Deleted %s\n", argv[1]);
            console_set_color(COLOR_TEXT);
        }
    } else if (strcasecmp(cmd, "cp") == 0) {
        if (argc < 3) console_write_line("usage: cp <src> <dst>");
        else {
            fs_file_t *src = fs_find(argv[1]);
            if (!src) {
                console_printf("cp: %s: no such file\n", argv[1]);
            } else if (!fs_create(argv[2])) {
                console_printf("cp: %s: already exists or bad name\n", argv[2]);
            } else {
                fs_write(argv[2], src->data, src->size);
                console_printf("Copied %s -> %s (%u bytes)\n", argv[1], argv[2], src->size);
            }
        }
    } else if (strcasecmp(cmd, "mv") == 0) {
        if (argc < 3) console_write_line("usage: mv <src> <dst>");
        else if (!fs_rename(argv[1], argv[2])) console_write_line("mv: failed (missing source or destination exists)");
        else console_printf("Renamed %s -> %s\n", argv[1], argv[2]);
    } else if (strcasecmp(cmd, "df") == 0) {
        cmd_df();
    } else if (strcasecmp(cmd, "stat") == 0) {
        if (argc < 2) console_write_line("usage: stat <name>");
        else cmd_stat(argv[1]);
    } else if (strcasecmp(cmd, "programs") == 0 || strcasecmp(cmd, "progs") == 0) {
        programs_list();
    } else if (strcasecmp(cmd, "start") == 0 || strcasecmp(cmd, "run") == 0) {
        if (argc < 2) console_write_line("usage: start <program.debil>");
        else programs_run(argv[1]);
    } else if (strcasecmp(cmd, "tasks") == 0 || strcasecmp(cmd, "taskmgr") == 0) {
        programs_run("tasks.debil");
    } else if (strcasecmp(cmd, "sysinfo") == 0 || strcasecmp(cmd, "sys") == 0) {
        cmd_sysinfo();
    } else if (strcasecmp(cmd, "meminfo") == 0 || strcasecmp(cmd, "mem") == 0) {
        mem_print_map();
        console_printf("Total usable: %u MB (%u KB)\n",
                       mem_total_kb() / 1024u, mem_total_kb());
    } else if (strcasecmp(cmd, "pci") == 0 || strcasecmp(cmd, "lspci") == 0) {
        cmd_pci();
    } else if (strcasecmp(cmd, "uptime") == 0) {
        console_write("Uptime: ");
        print_uptime();
        console_printf("   (%u ticks at %u Hz)\n", timer_ticks(), TIMER_HZ);
    } else if (strcasecmp(cmd, "date") == 0 || strcasecmp(cmd, "time") == 0) {
        cmd_date();
    } else if (strcasecmp(cmd, "history") == 0) {
        for (int i = 0; i < g_history_count; i++) {
            console_printf("%3d  %s\n", i + 1, g_history[i]);
        }
        if (g_history_count == 0) console_write_line("(empty)");
    } else if (strcasecmp(cmd, "stress") == 0) {
        cmd_stress(argc >= 2 ? argv[1] : 0);
    } else if (strcasecmp(cmd, "about") == 0 || strcasecmp(cmd, "version") == 0) {
        console_printf("DEBIL OS v%s - a 32-bit x86 terminal OS built with C and NASM.\n", g_version);
        console_write_line("Boot: custom MBR + protected mode + paging; no GRUB, no libc.");
    } else if (strcasecmp(cmd, "test_crash") == 0 || strcasecmp(cmd, "crash") == 0) {
        cmd_test_crash(argc >= 2 ? argv[1] : 0);
    } else if (strcasecmp(cmd, "reboot") == 0 || strcasecmp(cmd, "reset") == 0) {
        machine_reboot();
    } else if (strcasecmp(cmd, "shutdown") == 0 || strcasecmp(cmd, "poweroff") == 0 ||
               strcasecmp(cmd, "halt") == 0) {
        machine_shutdown();
    } else {
        console_set_color(COLOR_ERROR);
        console_printf("Unknown command: %s\n", raw);
        console_set_color(COLOR_TEXT);
        console_write_line("Type 'help' for the list of commands.");
    }
}

void shell_run(void)
{
    console_set_color(COLOR_TITLE);
    console_write_line("  ____  _____ ____ ___ _     ___  ____  ");
    console_write_line(" |  _ \\| ____| __ )_ _| |   / _ \\/ ___| ");
    console_write_line(" | | | |  _| |  _ \\| || |  | | | \\___ \\ ");
    console_write_line(" | |_| | |___| |_) | || |__| |_| |___) |");
    console_write_line(" |____/|_____|____/___|_____\\___/|____/ ");
    console_set_color(COLOR_TEXT);
    console_printf("DEBIL OS v%s  |  %s  |  %u MB RAM\n", g_version, cpu_brand(), mem_total_kb() / 1024u);

    if (pci_gpu_count()) {
        const pci_gpu_t *g = pci_gpu_get(0);
        console_printf("GPU: %s %04x:%04x   CPU cores: %u   ~%u MHz\n",
                       pci_vendor_name(g->vendor), g->vendor, g->device_id,
                       cpu_cores(), cpu_mhz());
    } else {
        console_printf("CPU cores: %u   ~%u MHz\n", cpu_cores(), cpu_mhz());
    }
    console_write_line("Type 'help' for commands, 'programs' for installed .debil programs.");
    console_write_line("Tip: 'test_crash cpu' intentionally reboots the machine to test boot.");
    console_write_line("");

    char line[SHELL_MAX_LINE];
    for (;;) {
        shell_print_prompt();
        int n = read_line_ex("", line, SHELL_MAX_LINE, 1);
        if (n < 0) continue;
        if (!line[0]) continue;
        history_add(line);
        shell_execute(line);
    }
}
