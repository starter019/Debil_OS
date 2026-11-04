/* DEBIL OS - VGA text console mirrored to an ANSI serial terminal */
#ifndef DEBIL_CONSOLE_H
#define DEBIL_CONSOLE_H

#include "types.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA text colours */
#define COLOR_BLACK         0x00
#define COLOR_BLUE          0x01
#define COLOR_GREEN         0x02
#define COLOR_CYAN          0x03
#define COLOR_RED           0x04
#define COLOR_MAGENTA       0x05
#define COLOR_BROWN         0x06
#define COLOR_LIGHT_GREY    0x07
#define COLOR_DARK_GREY     0x08
#define COLOR_LIGHT_BLUE    0x09
#define COLOR_LIGHT_GREEN   0x0A
#define COLOR_LIGHT_CYAN    0x0B
#define COLOR_LIGHT_RED     0x0C
#define COLOR_LIGHT_MAGENTA 0x0D
#define COLOR_YELLOW        0x0E
#define COLOR_WHITE         0x0F

#define COLOR_TEXT    ((COLOR_BLACK << 4) | COLOR_LIGHT_GREY)
#define COLOR_PROMPT  ((COLOR_BLACK << 4) | COLOR_LIGHT_GREEN)
#define COLOR_TITLE   ((COLOR_BLACK << 4) | COLOR_LIGHT_CYAN)
#define COLOR_ERROR   ((COLOR_BLACK << 4) | COLOR_LIGHT_RED)
#define COLOR_OK      ((COLOR_BLACK << 4) | COLOR_LIGHT_GREEN)
#define COLOR_WARN    ((COLOR_BLACK << 4) | COLOR_YELLOW)
#define COLOR_DIM     ((COLOR_BLACK << 4) | COLOR_DARK_GREY)

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_line(const char *s);
void console_set_color(uint8_t color);
uint8_t console_get_color(void);
void console_gotoxy(uint32_t x, uint32_t y);
uint32_t console_get_x(void);
uint32_t console_get_y(void);
void console_newline(void);
void console_update_cursor(void);
void console_printf(const char *fmt, ...);

/* Used by the task manager as a "GPU / VGA text activity" figure. */
uint32_t console_take_char_count(void);

#endif
