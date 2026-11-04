/* DEBIL OS - kernel main */
#include "types.h"
#include "console.h"
#include "idt.h"
#include "timer.h"
#include "input.h"
#include "mem.h"
#include "pci.h"
#include "cpu.h"
#include "fs.h"
#include "shell.h"
#include "string.h"
#include "io.h"

static void ok(const char *what)
{
    console_set_color(COLOR_OK);
    console_write("[ OK ] ");
    console_set_color(COLOR_TEXT);
    console_write_line(what);
}

static void warn(const char *what)
{
    console_set_color(COLOR_WARN);
    console_write("[WARN] ");
    console_set_color(COLOR_TEXT);
    console_write_line(what);
}

void kernel_main(void)
{
    console_init();

    console_set_color(COLOR_TITLE);
    console_write_line("DEBIL OS v1.0 - 32-bit x86 terminal operating system");
    console_set_color(COLOR_TEXT);
    console_write_line("Custom MBR boot, protected mode, 4 MB paging, no libc, no GRUB.");
    console_write_line("");

    idt_init();
    pic_remap();
    timer_init();
    input_init();
    pic_unmask(0);
    pic_unmask(1);
    pic_unmask(2);
    ok("IDT installed, 8259 PIC remapped to 0x20/0x28");
    ok("PIT timer running at 100 Hz");
    ok("PS/2 keyboard and COM1 serial input ready");

    cpu_sti();

    mem_init();
    if (mem_total_kb() > 0) {
        console_set_color(COLOR_OK);
        console_write("[ OK ] ");
        console_set_color(COLOR_TEXT);
        console_printf("E820 memory map: %u MB usable\n", mem_total_kb() / 1024u);
    } else {
        warn("E820 memory map unavailable, using BIOS fallback");
    }

    pci_init();
    console_set_color(COLOR_OK);
    console_write("[ OK ] ");
    console_set_color(COLOR_TEXT);
    console_printf("PCI scan: %u devices, %u display controller(s)\n",
                   pci_device_count(), pci_gpu_count());

    fs_init();
    console_set_color(COLOR_OK);
    console_write("[ OK ] ");
    console_set_color(COLOR_TEXT);
    console_printf("RAM filesystem: %u files seeded (%u bytes used)\n",
                   fs_count(), fs_used_bytes());

    cpu_init();
    console_set_color(COLOR_OK);
    console_write("[ OK ] ");
    console_set_color(COLOR_TEXT);
    console_printf("CPU: %s (%u cores, ~%u MHz)\n",
                   cpu_brand(), cpu_cores(), cpu_mhz());

    if (pci_gpu_count() > 0) {
        const pci_gpu_t *g = pci_gpu_get(0);
        console_printf("       GPU: %s %04x:%04x, VRAM %u KB\n",
                       pci_vendor_name(g->vendor), g->vendor, g->device_id,
                       g->bar0_size / 1024u);
    } else {
        console_write_line("       GPU: no PCI display controller detected");
    }

    console_write_line("");
    console_set_color(COLOR_DIM);
    console_write_line("Filesystem is RAM-backed: changes are lost on reset.");
    console_set_color(COLOR_TEXT);
    console_write_line("");

    shell_run();

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
