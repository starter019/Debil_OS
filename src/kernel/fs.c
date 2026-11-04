/* DEBIL OS - in-memory filesystem (24 files x 4 KiB) */
#include "fs.h"
#include "string.h"

static fs_file_t g_files[FS_MAX_FILES];

int fs_valid_name(const char *name)
{
    if (!name || !*name) return 0;
    size_t n = strlen(name);
    if (n >= FS_MAX_NAME) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return 0;
        }
    }
    return 1;
}

uint32_t fs_count(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].used) n++;
    }
    return n;
}

fs_file_t *fs_get(uint32_t index)
{
    if (index >= FS_MAX_FILES || !g_files[index].used) return 0;
    return &g_files[index];
}

fs_file_t *fs_find(const char *name)
{
    if (!name) return 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].used && strcasecmp(g_files[i].name, name) == 0) {
            return &g_files[i];
        }
    }
    return 0;
}

static fs_file_t *find_free_slot(void)
{
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!g_files[i].used) return &g_files[i];
    }
    return 0;
}

fs_file_t *fs_create(const char *name)
{
    if (!fs_valid_name(name)) return 0;
    if (fs_find(name)) return 0;
    fs_file_t *f = find_free_slot();
    if (!f) return 0;
    memset(f, 0, sizeof(*f));
    f->used = 1;
    strncpy(f->name, name, FS_MAX_NAME - 1);
    f->name[FS_MAX_NAME - 1] = 0;
    f->size = 0;
    f->data[0] = 0;
    return f;
}

int fs_delete(const char *name)
{
    fs_file_t *f = fs_find(name);
    if (!f) return 0;
    memset(f, 0, sizeof(*f));
    return 1;
}

int fs_write(const char *name, const void *data, uint32_t len)
{
    fs_file_t *f = fs_find(name);
    if (!f) return 0;
    if (len > FS_MAX_DATA - 1) len = FS_MAX_DATA - 1;
    if (len) memcpy(f->data, data, len);
    f->data[len] = 0;
    f->size = len;
    return 1;
}

int fs_append(const char *name, const void *data, uint32_t len)
{
    fs_file_t *f = fs_find(name);
    if (!f) return 0;
    if (len > FS_MAX_DATA - 1 - f->size) len = FS_MAX_DATA - 1 - f->size;
    if (len) memcpy(f->data + f->size, data, len);
    f->size += len;
    f->data[f->size] = 0;
    return 1;
}

int fs_rename(const char *old_name, const char *new_name)
{
    if (!fs_valid_name(new_name)) return 0;
    if (fs_find(new_name)) return 0;
    fs_file_t *f = fs_find(old_name);
    if (!f) return 0;
    strncpy(f->name, new_name, FS_MAX_NAME - 1);
    f->name[FS_MAX_NAME - 1] = 0;
    return 1;
}

uint32_t fs_used_bytes(void)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].used) total += g_files[i].size;
    }
    return total;
}

uint32_t fs_capacity_bytes(void)
{
    return FS_MAX_FILES * (FS_MAX_DATA - 1);
}

static void seed(const char *name, const char *text)
{
    fs_file_t *f = fs_create(name);
    if (f) fs_write(name, text, (uint32_t)strlen(text));
}

void fs_init(void)
{
    memset(g_files, 0, sizeof(g_files));

    seed("readme.txt",
         "Welcome to DEBIL OS!\n"
         "\n"
         "This is a small 32-bit terminal operating system written in C and NASM.\n"
         "Type 'help' for the command list and 'programs' for installed programs.\n"
         "\n"
         "Useful things to try:\n"
         "  ls\n"
         "  mkfile notes.txt\n"
         "  edit notes.txt        (inside the editor use :w to save, :q to quit)\n"
         "  cat notes.txt\n"
         "  start snake.debil\n"
         "  start tasks.debil\n"
         "  test_crash cpu        (this really reboots the machine)\n"
         "  shutdown\n");

    seed("snake.debil",
         "Built-in DEBIL program: SNAKE\n"
         "Start with: start snake.debil\n"
         "Controls: arrow keys or WASD, P pause, ESC quit.\n");

    seed("tasks.debil",
         "Built-in DEBIL program: TASK MANAGER\n"
         "Start with: start tasks.debil\n"
         "Shows CPU, RAM and GPU information with live updates.\n");

    seed("crash.debil",
         "Built-in DEBIL program: CRASH TESTER\n"
         "Start with: start crash.debil\n"
         "Lets you inject a CPU, RAM or GPU fault and watch the reboot.\n");

    seed("hello.debil",
         "Built-in DEBIL program: HELLO\n"
         "Start with: start hello.debil\n");

    seed("calc.debil",
         "Built-in DEBIL program: CALCULATOR\n"
         "Start with: start calc.debil\n"
         "Example input: 12 * 7\n");
}
