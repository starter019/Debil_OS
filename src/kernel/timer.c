/* DEBIL OS - PIT timer at 100 Hz + cooperative idle accounting */
#include "timer.h"
#include "console.h"
#include "io.h"

#define PIT_FREQUENCY 1193182u
#define PIT_DIVISOR   (PIT_FREQUENCY / TIMER_HZ)

static volatile uint32_t g_ticks = 0;
static volatile uint32_t g_busy_ticks = 0;
static volatile uint32_t g_idle_ticks = 0;
static volatile uint32_t g_cpu_percent = 0;
static volatile uint32_t g_chars_per_sec = 0;
static volatile int g_idle_flag = 0;

void timer_init(void)
{
    uint16_t divisor = (uint16_t)PIT_DIVISOR;

    outb(0x43, 0x36);                      /* channel 0, lobyte/hibyte, mode 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)(divisor >> 8));
}

void timer_irq(void)
{
    g_ticks++;

    if (g_idle_flag) g_idle_ticks++;
    else             g_busy_ticks++;

    if ((g_ticks % TIMER_HZ) == 0) {
        uint32_t total = g_busy_ticks + g_idle_ticks;
        if (total == 0) total = 1;
        g_cpu_percent = (g_busy_ticks * 100u) / total;
        if (g_cpu_percent > 100) g_cpu_percent = 100;
        g_busy_ticks = 0;
        g_idle_ticks = 0;

        g_chars_per_sec = console_take_char_count();
    }
}

uint32_t timer_ticks(void)
{
    return g_ticks;
}

uint32_t timer_uptime_seconds(void)
{
    return g_ticks / TIMER_HZ;
}

uint32_t timer_cpu_percent(void)
{
    return g_cpu_percent;
}

uint32_t timer_chars_per_sec(void)
{
    return g_chars_per_sec;
}

void cpu_idle(void)
{
    g_idle_flag = 1;
    cpu_wait_interrupt();
    g_idle_flag = 0;
}

void timer_sleep_ms(uint32_t ms)
{
    uint32_t target = g_ticks + (ms / (1000 / TIMER_HZ));
    if (target == g_ticks) target++;
    while ((int32_t)(g_ticks - target) < 0) {
        cpu_idle();
    }
}
