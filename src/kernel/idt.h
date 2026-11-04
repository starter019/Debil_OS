/* DEBIL OS - IDT, PIC and interrupt dispatch */
#ifndef DEBIL_IDT_H
#define DEBIL_IDT_H

#include "types.h"

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, err, eip, cs, eflags;
} PACKED regs_t;

void idt_init(void);
void pic_remap(void);
void pic_unmask(uint8_t irq);
void pic_mask(uint8_t irq);

#endif
