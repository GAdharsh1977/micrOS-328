#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/sleep.h>

#include "time.h"

#define TIMER1_PRESCALER 64U

void timer_init(void)
{
    TCCR1A = 0;
    TCCR1B = 0;

    TCNT1 = 0;

    TIFR1 = (1 << OCF1A);

    TIMSK1 = (1 << TOIE1);

    TCCR1B = (1 << CS11) | // not in ctc mode.
             (1 << CS10);

    sei();
}

uint32_t timer_get_hardware_ticks(void){
    uint32_t high;
    uint16_t low;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
        low = TCNT1;
        high = timer1_overflow_count;
        if(TIFR1 & (1 << TOV1) && low < 32768){
            high++;
        }
    }
    return (uint32_t)((high << 16) | low);
}

void sleep_until(uint32_t timestamp){
    while(1){
        uint32_t now = timer_get_hardware_ticks();
        int32_t remaining = timestamp - now;
        if(remaining <= 0) break;

        uint16_t sleep_slice = 60000U;
        if((uint32_t)(remaining) < 60000UL){
            sleep_slice = (uint16_t)remaining;
        }

        cli();
        OCR1A = (uint16_t)(TCNT1 + sleep_slice);
        TIFR1 = (1 << OCF1A);
        TIMSK1 |= (1 << OCIE1A);
        sleep_enable();
        
        sei();
        sleep_cpu();
        sleep_disable();
        TIMSK1 &= ~(1 << OCIE1A);
    }
}