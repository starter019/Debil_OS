/* DEBIL OS - PS/2 keyboard + serial input */
#ifndef DEBIL_INPUT_H
#define DEBIL_INPUT_H

#include "types.h"

/* Non-ASCII key codes. Printable keys are returned as their ASCII value. */
#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_ESC       0x105
#define KEY_ENTER     0x106
#define KEY_BACKSPACE 0x107
#define KEY_TAB       0x108
#define KEY_DELETE    0x109
#define KEY_HOME      0x10A
#define KEY_END       0x10B
#define KEY_CTRLC     0x10C
#define KEY_F1        0x110
#define KEY_F2        0x111
#define KEY_F3        0x112
#define KEY_F4        0x113
#define KEY_PAGEUP    0x114
#define KEY_PAGEDOWN  0x115

void input_init(void);
void input_keyboard_irq(void);
int  input_poll(void);
int  input_wait(void);
int  input_available(void);

#endif
