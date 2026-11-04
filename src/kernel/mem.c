/* DEBIL OS - E820 memory map parsing */
#include "mem.h"
#include "console.h"
#include "string.h"

extern uint8_t kernel_end;

static boot_info_t *const g_boot = (boot_info_t *)BOOT_INFO_ADDR;
static e820_entry_t *const g_map = (e820_entry_t *)E820_ADDR;
static uint32_t g_count = 0;
static uint32_t g_total_kb = 0;
static uint32_t g_lower_kb = 0;

void mem_init(void)
{
    g_count = 0;
    g_total_kb = 0;
    g_lower_kb = 0;

    if (g_boot->magic == 0xDEB11055 && g_boot->mmap_addr == E820_ADDR) {
        g_count = g_boot->mmap_count;
        if (g_count > E820_MAX) g_count = E820_MAX;
        g_lower_kb = g_boot->mem_lower_kb;

        for (uint32_t i = 0; i < g_count; i++) {
            e820_entry_t *e = &g_map[i];
            if (e->type == 1) {
                uint32_t len_lo = (uint32_t)e->length;
                if (len_lo > 0xFFFFF000u) len_lo = 0xFFFFF000u;
                g_total_kb += len_lo / 1024u;
            }
        }
    }

    if (g_total_kb == 0) {
        g_total_kb = g_boot->mem_lower_kb + g_boot->mem_upper_kb;
        g_lower_kb = g_boot->mem_lower_kb;
    }
}

uint32_t mem_total_kb(void)      { return g_total_kb; }
uint32_t mem_lower_kb(void)      { return g_lower_kb; }
uint32_t mem_e820_count(void)    { return g_count; }
const e820_entry_t *mem_e820_map(void) { return g_map; }

uint32_t mem_kernel_end(void)
{
    return (uint32_t)&kernel_end;
}

uint32_t mem_kernel_bytes(void)
{
    uint32_t end = (uint32_t)&kernel_end;
    if (end < 0x100000u) return 0;
    return end - 0x100000u;
}

static const char *type_name(uint32_t type)
{
    switch (type) {
    case 1: return "usable";
    case 2: return "reserved";
    case 3: return "ACPI reclaim";
    case 4: return "ACPI NVS";
    case 5: return "bad memory";
    default: return "other";
    }
}

void mem_print_map(void)
{
    console_printf("E820 map: %u entries (kernel ends at 0x%x)\n",
                   g_count, mem_kernel_end());
    console_write_line("  BASE(KB)   LENGTH(KB)   TYPE");
    for (uint32_t i = 0; i < g_count; i++) {
        const e820_entry_t *e = &g_map[i];
        uint32_t base_kb = (uint32_t)(e->base >> 10);
        uint32_t len_kb = (uint32_t)(e->length >> 10);
        console_printf("  %9u   %10u   %s\n", base_kb, len_kb, type_name(e->type));
    }
}
