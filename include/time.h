#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#define TICKS_PER_MS 250U
#define MS_TO_TICKS(ms) ((uint32_t)(ms) * TICKS_PER_MS)
#define TICKS_TO_MS(t)  ((uint32_t)(t) / TICKS_PER_MS)

extern volatile uint16_t timer1_overflow_count;

void timer_init(void);
uint32_t timer_get_hardware_ticks(void);
void sleep_until(uint32_t timestamp);

#endif