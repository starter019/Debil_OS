/* DEBIL OS - COM1 serial console (also lets the OS run under -nographic) */
#ifndef DEBIL_SERIAL_H
#define DEBIL_SERIAL_H

#include "types.h"

void serial_init(void);
int  serial_ok(void);
void serial_putc(char c);
void serial_write(const char *s);
int  serial_available(void);
char serial_read(void);

#endif
