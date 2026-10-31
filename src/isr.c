#include <avr/interrupt.h>
#include "time.h"
#include<stdint.h>
#include "functions.h"

// volatile uint8_t disable_counter_requested = 0; // compiler optimisation prevention.
volatile uint8_t disable_breathing_requested = 0;
// volatile uint8_t disable_blinking_requested = 0;
volatile uint8_t trigger_led_pattern_requested = 0;
static uint32_t last_trigger_0 = 0;
static uint32_t last_trigger_1 = 0;

ISR(INT0_vect){
    uint32_t now = timer_get_hardware_ticks();
    if((uint32_t)(now - last_trigger_0) < MS_TO_TICKS(2000)){
        return;
    }
    last_trigger_0 = now;
    trigger_led_pattern_requested = 1;
    task_entry_t* sup = get_task_by_id(supervisor_task_id);
    if(sup != NULL){
        sup->state = TASK_READY;
        sup->next_run = now;
        next_task_to_run = sup;
    }
    task_yield();
}

ISR(INT1_vect){
    uint32_t now = timer_get_hardware_ticks();
    if((uint32_t)(now - last_trigger_1) < MS_TO_TICKS(250)){
        return;
    }
    last_trigger_1 = now;
    disable_breathing_requested = 2;
    task_entry_t* sup = get_task_by_id(supervisor_task_id);
    if(sup != NULL){
        sup->state = TASK_READY;
        sup->next_run = now;
        next_task_to_run = sup;
    }
    task_yield();
}

volatile uint16_t timer1_overflow_count = 0;
ISR(TIMER1_OVF_vect)
{
    timer1_overflow_count++;
}

ISR(TIMER1_COMPA_vect){
    TIMSK1 &= ~(1 << OCIE1A);
}
ISR(TIMER1_COMPB_vect){
    TIMSK1 &= ~(1 << OCIE1B);
    task_yield();
}