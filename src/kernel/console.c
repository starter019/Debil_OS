/* DEBIL OS - VGA text console + ANSI serial mirror + kprintf */
#include "console.h"
#include "serial.h"
#include "string.h"
#include "io.h"

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;

static uint32_t g_cx = 0;
static uint32_t g_cy = 0;
static uint8_t  g_color = COLOR_TEXT;
static uint8_t  g_serial_color = 0xFF;
static uint32_t g_chars_written = 0;

static void serial_sgr(uint8_t color)
{
    if (!serial_ok() || color == g_serial_color) return;

    uint8_t fg = color & 0x0F;
    uint8_t bg = (color >> 4) & 0x0F;
    char buf[24];
    int n = 0;

    buf[n++] = 0x1B;
    buf[n++] = '[';
    buf[n++] = '0';
    buf[n++] = ';';

    uint32_t fgcode = (fg & 8) ? 90 + (fg & 7) : 30 + (fg & 7);
    uint32_t bgcode = (bg & 8) ? 100 + (bg & 7) : 40 + (bg & 7);

    char num[8];
    utoa(fgcode, num, 10);
    for (int i = 0; num[i]; i++) buf[n++] = num[i];
    buf[n++] = ';';
    utoa(bgcode, num, 10);
    for (int i = 0; num[i]; i++) buf[n++] = num[i];
    buf[n++] = 'm';
    buf[n] = 0;

    serial_write(buf);
    g_serial_color = color;
}

void console_update_cursor(void)
{
    uint16_t pos = (uint16_t)(g_cy * VGA_WIDTH + g_cx);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void console_scroll(void)
{
    memmove((void *)VGA, (const void *)(VGA + VGA_WIDTH),
            (VGA_WIDTH * (VGA_HEIGHT - 1)) * sizeof(uint16_t));
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)g_color << 8));
    for (uint32_t x = 0; x < VGA_WIDTH; x++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

void console_clear(void)
{
    g_color = COLOR_TEXT;
    serial_sgr(g_color);
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)g_color << 8));
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA[i] = blank;
    }
    g_cx = 0;
    g_cy = 0;
    console_update_cursor();

    if (serial_ok()) {
        serial_write("\x1b[0m\x1b[2J\x1b[H");
        g_serial_color = 0xFF;
    }
}

void console_init(void)
{
    serial_init();
    console_clear();
    if (serial_ok()) {
        serial_write("\x1b[0m");
        g_serial_color = 0xFF;
    }
}

void console_set_color(uint8_t color)
{
    g_color = color;
    serial_sgr(color);
}

uint8_t console_get_color(void)
{
    return g_color;
}

void console_gotoxy(uint32_t x, uint32_t y)
{
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    g_cx = x;
    g_cy = y;
    console_update_cursor();

    if (serial_ok()) {
        char buf[28];
        int n = 0;
        buf[n++] = 0x1B;
        buf[n++] = '[';
        char num[8];
        utoa(y + 1, num, 10);
        for (int i = 0; num[i]; i++) buf[n++] = num[i];
        buf[n++] = ';';
        utoa(x + 1, num, 10);
        for (int i = 0; num[i]; i++) buf[n++] = num[i];
        buf[n++] = 'H';
        buf[n] = 0;
        serial_write(buf);
    }
}

uint32_t console_get_x(void) { return g_cx; }
uint32_t console_get_y(void) { return g_cy; }

void console_newline(void)
{
    g_cx = 0;
    g_cy++;
    if (g_cy >= VGA_HEIGHT) {
        g_cy = VGA_HEIGHT - 1;
        console_scroll();
    }
    console_update_cursor();
}

void console_putc(char c)
{
    if (c == '\n') {
        serial_putc('\n');
        console_newline();
        return;
    }
    if (c == '\r') {
        g_cx = 0;
        console_update_cursor();
        return;
    }
    if (c == '\t') {
        uint32_t next = (g_cx + 4) & ~3u;
        while (g_cx < next) console_putc(' ');
        return;
    }
    if (c == '\b') {
        if (g_cx > 0) g_cx--;
        VGA[g_cy * VGA_WIDTH + g_cx] = (uint16_t)(' ' | ((uint16_t)g_color << 8));
        serial_write("\b \b");
        console_update_cursor();
        return;
    }

    VGA[g_cy * VGA_WIDTH + g_cx] = (uint16_t)((uint8_t)c | ((uint16_t)g_color << 8));
    serial_putc(c);
    g_chars_written++;

    g_cx++;
    if (g_cx >= VGA_WIDTH) {
        g_cx = 0;
        g_cy++;
        if (g_cy >= VGA_HEIGHT) {
            g_cy = VGA_HEIGHT - 1;
            console_scroll();
        }
    }
    console_update_cursor();
}

void console_write(const char *s)
{
    while (*s) console_putc(*s++);
}

void console_write_line(const char *s)
{
    console_write(s);
    console_putc('\n');
}

uint32_t console_take_char_count(void)
{
    uint32_t n = g_chars_written;
    g_chars_written = 0;
    return n;
}

/* ------------------------------------------------------------------ */
/* Minimal printf: %s %c %d %u %x %X %p %b %% with width/left/zero pad  */
/* ------------------------------------------------------------------ */

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, l)   __builtin_va_arg(v, l)
#define va_end(v)      __builtin_va_end(v)

static void emit_pad(char c, int count)
{
    while (count-- > 0) console_putc(c);
}

static void emit_string(const char *s, int width, int left)
{
    int len = (int)strlen(s);
    if (!left) emit_pad(' ', width - len);
    console_write(s);
    if (left) emit_pad(' ', width - len);
}

static void emit_number(uint32_t value, int base, int width, int left, int zero,
                        int is_signed_value, int32_t signed_value,
                        int uppercase, int prefix)
{
    char tmp[40];
    char out[48];
    int n = 0;
    int negative = 0;

    if (is_signed_value && signed_value < 0) {
        negative = 1;
        value = (uint32_t)(-signed_value);
    }

    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value && n < 32) {
            uint32_t d = value % (uint32_t)base;
            if (d < 10) tmp[n++] = (char)('0' + d);
            else tmp[n++] = (char)((uppercase ? 'A' : 'a') + d - 10);
            value /= (uint32_t)base;
        }
    }

    int pos = 0;
    if (negative) out[pos++] = '-';
    else if (prefix) {
        out[pos++] = '0';
        out[pos++] = uppercase ? 'X' : 'x';
    }
    int digits = n;
    int total = pos + digits;
    if (!left) {
        if (zero && !negative) {
            if (prefix) { console_putc('0'); console_putc(uppercase ? 'X' : 'x'); }
            emit_pad('0', width - total);
        } else {
            emit_pad(' ', width - total);
            for (int i = 0; i < pos; i++) console_putc(out[i]);
        }
    }
    if (left || !zero || negative) {
        for (int i = 0; i < pos; i++) console_putc(out[i]);
    }
    while (n > 0) console_putc(tmp[--n]);
    if (left) emit_pad(' ', width - total);
}

void console_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            console_putc('%');
            fmt++;
            continue;
        }

        int left = 0, zero = 0, width = 0;
        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left = 1;
            if (*fmt == '0') zero = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        while (*fmt == 'l' || *fmt == 'z') fmt++;

        char ch = *fmt++;
        switch (ch) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            emit_string(s, width, left);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!left && width > 1) emit_pad(' ', width - 1);
            console_putc(c);
            if (left && width > 1) emit_pad(' ', width - 1);
            break;
        }
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            emit_number(v < 0 ? (uint32_t)(-v) : (uint32_t)v, 10, width, left, zero, 1, v, 0, 0);
            break;
        }
        case 'u': {
            uint32_t v = va_arg(ap, uint32_t);
            emit_number(v, 10, width, left, zero, 0, 0, 0, 0);
            break;
        }
        case 'x': {
            uint32_t v = va_arg(ap, uint32_t);
            emit_number(v, 16, width, left, zero, 0, 0, 0, 0);
            break;
        }
        case 'X': {
            uint32_t v = va_arg(ap, uint32_t);
            emit_number(v, 16, width, left, zero, 0, 0, 1, 0);
            break;
        }
        case 'p': {
            uint32_t v = va_arg(ap, uint32_t);
            console_write("0x");
            emit_number(v, 16, 8, 0, 1, 0, 0, 0, 0);
            break;
        }
        case 'b': {
            uint32_t v = va_arg(ap, uint32_t);
            emit_number(v, 2, width, left, zero, 0, 0, 0, 0);
            break;
        }
        default:
            console_putc('%');
            if (ch) console_putc(ch);
            break;
        }
    }

    va_end(ap);
}
