/* DEBIL OS - PIT timer, uptime and CPU load accounting */
#ifndef DEBIL_TIMER_H
#define DEBIL_TIMER_H

#include "types.h"

#define TIMER_HZ 100

void timer_init(void);
void timer_irq(void);
uint32_t timer_ticks(void);
uint32_t timer_uptime_seconds(void);
uint32_t timer_cpu_percent(void);
uint32_t timer_chars_per_sec(void);
void timer_sleep_ms(uint32_t ms);
void cpu_idle(void);

#endif
