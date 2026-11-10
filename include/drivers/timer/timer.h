#ifndef VEOOS_DRIVERS_TIMER_TIMER_H
#define VEOOS_DRIVERS_TIMER_TIMER_H

#include "util/types.h"

void timer_initialize(void);
void timer_sleep_ms(uint32_t ms);
void timer_sleep_us(uint32_t us);
uint64_t timer_get_uptime_ms(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_cpu_freq_hz(void);

#endif
