/* DEBIL OS - IDT + 8259 PIC + interrupt dispatch */
#include "idt.h"
#include "io.h"
#include "timer.h"
#include "input.h"
#include "panic.h"

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} PACKED idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idt_ptr_t;

static idt_entry_t g_idt[256];
static idt_ptr_t   g_idt_ptr;

extern uint32_t isr_stub_table[48];
extern void isr_default(void);

static void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags)
{
    g_idt[vector].base_low  = (uint16_t)(handler & 0xFFFF);
    g_idt[vector].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    g_idt[vector].selector  = selector;
    g_idt[vector].zero      = 0;
    g_idt[vector].flags     = flags;
}

void pic_remap(void)
{
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t value;
    if (irq < 8) {
        port = 0x21;
        value = inb(port) & (uint8_t)~(1u << irq);
    } else {
        port = 0xA1;
        value = inb(port) & (uint8_t)~(1u << (irq - 8));
        outb(0x21, inb(0x21) & (uint8_t)~(1u << 2));
    }
    outb(port, value);
}

void pic_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t value;
    if (irq < 8) {
        port = 0x21;
        value = inb(port) | (uint8_t)(1u << irq);
    } else {
        port = 0xA1;
        value = inb(port) | (uint8_t)(1u << (irq - 8));
    }
    outb(port, value);
}

void idt_init(void)
{
    g_idt_ptr.limit = (uint16_t)(sizeof(g_idt) - 1);
    g_idt_ptr.base  = (uint32_t)&g_idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)isr_default, 0x08, 0x8E);
    }
    for (int i = 0; i < 48; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E);
    }

    __asm__ volatile("lidt %0" : : "m"(g_idt_ptr));
}

static void pic_eoi(uint8_t irq)
{
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void isr_handler(regs_t *r)
{
    uint32_t v = r->vector;

    if (v >= 32 && v < 48) {
        uint8_t irq = (uint8_t)(v - 32);
        if (irq == 0) timer_irq();
        else if (irq == 1) input_keyboard_irq();
        pic_eoi(irq);
        return;
    }

    if (v >= 48) {
        /* Unexpected/spurious vector: acknowledge both PICs and ignore. */
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    panic_dump(r);
}
