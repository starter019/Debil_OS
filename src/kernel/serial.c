/* DEBIL OS - COM1 serial driver (polled, 115200 8N1) */
#include "serial.h"
#include "io.h"

#define COM1 0x3F8

static int g_serial_ok = 0;

static int tx_ready(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(COM1 + 5) & 0x20) return 1;
    }
    return 0;
}

void serial_init(void)
{
    g_serial_ok = 0;

    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* enable DLAB */
    outb(COM1 + 0, 0x01);   /* divisor low: 115200 baud */
    outb(COM1 + 1, 0x00);   /* divisor high */
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO enable + clear */
    outb(COM1 + 4, 0x0B);   /* DTR, RTS, OUT2 */

    /* Loopback self test. If there is no UART this fails harmlessly. */
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    for (uint32_t i = 0; i < 10000; i++) {
        if (inb(COM1 + 5) & 0x01) break;
    }
    if (inb(COM1 + 0) == 0xAE) {
        g_serial_ok = 1;
    }
    outb(COM1 + 4, 0x0F);   /* normal mode */
}

int serial_ok(void)
{
    return g_serial_ok;
}

void serial_putc(char c)
{
    if (!g_serial_ok) return;
    if (c == '\n') {
        if (tx_ready()) outb(COM1, '\r');
    }
    if (tx_ready()) outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s) serial_putc(*s++);
}

int serial_available(void)
{
    if (!g_serial_ok) return 0;
    return (inb(COM1 + 5) & 0x01) != 0;
}

char serial_read(void)
{
    while (!serial_available()) { }
    return (char)inb(COM1);
}
