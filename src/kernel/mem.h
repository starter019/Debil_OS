/* DEBIL OS - E820 memory map and kernel footprint */
#ifndef DEBIL_MEM_H
#define DEBIL_MEM_H

#include "types.h"

#define BOOT_INFO_ADDR 0x7F00
#define E820_ADDR      0x8000
#define E820_MAX       32

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} PACKED e820_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t mmap_addr;
    uint32_t mmap_count;
    uint32_t mem_lower_kb;
    uint32_t mem_upper_kb;
    uint8_t  boot_drive;
    uint8_t  pad[3];
} PACKED boot_info_t;

void mem_init(void);
uint32_t mem_total_kb(void);
uint32_t mem_lower_kb(void);
uint32_t mem_kernel_bytes(void);
uint32_t mem_kernel_end(void);
uint32_t mem_e820_count(void);
const e820_entry_t *mem_e820_map(void);
void mem_print_map(void);

#endif
