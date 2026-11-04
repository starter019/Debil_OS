/* DEBIL OS - panic screen, intentional crash tests and reset */
#include "panic.h"
#include "console.h"
#include "timer.h"
#include "io.h"

static volatile int g_crash_mode = CRASH_NONE;

static const char *exception_names[32] = {
    "Divide-by-zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

static const char *crash_name(int mode)
{
    switch (mode) {
    case CRASH_CPU: return "CPU (invalid opcode)";
    case CRASH_RAM: return "RAM (page fault on unmapped memory)";
    case CRASH_GPU: return "GPU (page fault on GPU MMIO range)";
    default:        return "unknown";
    }
}

int crash_mode(void)
{
    return g_crash_mode;
}

void machine_reboot(void)
{
    console_set_color(COLOR_WARN);
    console_write_line("");
    console_write_line("  >>> resetting the machine <<<");
    console_set_color(COLOR_TEXT);

    /* Pulse the 8042 reset line. This is what QEMU, Bochs and VirtualBox use. */
    for (int i = 0; i < 20; i++) {
        if ((inb(0x64) & 0x02) == 0) break;
        io_wait();
    }
    outb(0x64, 0xFE);

    for (volatile uint32_t i = 0; i < 20000000; i++) { }

    /* Fallback: load a null IDT and cause a triple fault, which resets the CPU. */
    struct {
        uint16_t limit;
        uint32_t base;
    } PACKED null_idt = { 0, 0 };

    __asm__ volatile("lidt %0" : : "m"(null_idt));
    __asm__ volatile("int $3");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void machine_shutdown(void)
{
    console_set_color(COLOR_WARN);
    console_write_line("");
    console_write_line("  >>> powering off <<<");
    console_set_color(COLOR_TEXT);

    /* ACPI power-off used by QEMU, Bochs and VirtualBox. Harmless elsewhere. */
    outw(0x604, 0x2000);    /* QEMU (acpi pm1a_cnt) */
    outw(0xB004, 0x2000);   /* older QEMU/Bochs */
    outw(0x4004, 0x3400);   /* VirtualBox */

    console_write_line("ACPI power-off was not accepted.");
    console_write_line("It is now safe to turn off the computer.");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void crash_trigger(int mode)
{
    g_crash_mode = mode;

    console_set_color(COLOR_ERROR);
    console_write_line("");
    console_write_line("==================================================================");
    console_write_line("  INTENTIONAL SYSTEM CRASH TEST - THIS IS NOT A REAL BUG");
    console_write_line("==================================================================");
    console_printf("  Fault to inject : %s\n", crash_name(mode));
    console_write_line("  The exception handler should catch it, print a register dump");
    console_write_line("  and reset the machine so you can watch the reboot path.");
    console_write_line("");
    console_printf("  Injecting fault in 1 second...\n");
    console_set_color(COLOR_TEXT);

    timer_sleep_ms(1000);

    if (mode == CRASH_CPU) {
        __asm__ volatile("ud2");
    } else if (mode == CRASH_RAM) {
        volatile uint32_t *bad = (volatile uint32_t *)0x40000000u;
        *bad = 0xDEADBEEFu;
    } else if (mode == CRASH_GPU) {
        volatile uint32_t *gpu = (volatile uint32_t *)0xE0000000u;
        *gpu = 0xDEADBEEFu;
    }

    for (;;) { }
}

void panic_dump(regs_t *r)
{
    cpu_cli();

    uint32_t cr2 = read_cr2();
    uint32_t cr3 = read_cr3();
    uint32_t cr0 = read_cr0();
    uint32_t cr4 = read_cr4();

    console_set_color(COLOR_ERROR);
    console_write_line("");
    console_write_line("##################################################################");
    console_write_line("#                    D E B I L   P A N I C                      #");
    console_write_line("##################################################################");
    console_set_color(COLOR_TEXT);

    if (r->vector < 32) {
        console_printf("Exception : %u - %s\n", r->vector, exception_names[r->vector]);
    } else {
        console_printf("Exception : vector %u\n", r->vector);
    }

    console_printf("Error code: 0x%x\n", r->err);
    console_printf("EIP       : 0x%x   CS: 0x%x   EFLAGS: 0x%x\n", r->eip, r->cs, r->eflags);

    if (r->vector == 14) {
        console_printf("CR2       : 0x%x  (faulting address)\n", cr2);
    }
    console_printf("CR0=%x  CR3=%x  CR4=%x\n", cr0, cr3, cr4);

    console_write_line("");
    console_printf("EAX=%x EBX=%x ECX=%x EDX=%x\n", r->eax, r->ebx, r->ecx, r->edx);
    console_printf("ESI=%x EDI=%x EBP=%x ESP=%x\n", r->esi, r->edi, r->ebp, r->esp);
    console_write_line("");

    if (g_crash_mode != CRASH_NONE) {
        console_set_color(COLOR_OK);
        console_write_line("  +------------------------------------------------------------+");
        console_printf ("  |  CRASH TEST PASSED: %-38s |\n", crash_name(g_crash_mode));
        console_write_line("  |  The CPU faulted, the kernel caught it and stayed alive.   |");
        console_write_line("  |  Rebooting in 3 seconds to test the reboot path...          |");
        console_write_line("  +------------------------------------------------------------+");
        console_set_color(COLOR_TEXT);

        /* The exception gate cleared IF. Re-enable it so the PIT can wake us. */
        cpu_sti();
        for (int s = 3; s > 0; s--) {
            console_printf("  reboot in %d...\n", s);
            timer_sleep_ms(1000);
        }
        machine_reboot();
    }

    console_set_color(COLOR_WARN);
    console_write_line("The system is halted. Reset the machine (or restart QEMU) to boot again.");
    console_set_color(COLOR_TEXT);

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
