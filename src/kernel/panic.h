/* DEBIL OS - panic screen, crash test injection and machine reset */
#ifndef DEBIL_PANIC_H
#define DEBIL_PANIC_H

#include "types.h"
#include "idt.h"

#define CRASH_NONE 0
#define CRASH_CPU  1
#define CRASH_RAM  2
#define CRASH_GPU  3

void panic_dump(regs_t *r);
void crash_trigger(int mode);
int  crash_mode(void);
void machine_reboot(void) NORETURN;
void machine_shutdown(void) NORETURN;

#endif
