#include <avr/io.h>
#include <stdint.h>
#include <stddef.h>
#include <avr/sleep.h>

#include "time.h"
#include "scheduler.h"
#include "interrupt.h"
#include "task.h"
#include "pwm.h"
#include "functions.h"

mutex_t portb_mutex;
mutex_t portc_mutex;
mutex_t portd_mutex;
scheduler_status_t status;

static void gpio_init(void)
{
    DDRB |= COUNTER_MASK;
    DDRB |= (1U << ERROR_LED);
    DDRB |= (1U << BLINK_LED);
    DDRD &= ~(1 << PD2); // Set PD2 (INT0) as input
    DDRD &= ~(1 << PD3); // Set PD3 (INT1) as input
    DDRD |= (1U << PWM_LED);
    DDRD |= (1U << POWER_ON_LED);

    PORTB &= (uint8_t)~COUNTER_MASK;
    PORTB &= (uint8_t)~(1U << BLINK_LED);
    PORTB &= (uint8_t)~(1U << ERROR_LED);
    PORTD &= (uint8_t)~(1U << PWM_LED);
    PORTD |= (uint8_t)(1U << POWER_ON_LED);
}

int main(void)
{
    gpio_init();
    timer_init();
    scheduler_init(&status);
    enable_INT0();
    enable_INT1();
    pwm_led_init();
    mutex_init(&portb_mutex);
    mutex_init(&portc_mutex);
    mutex_init(&portd_mutex);

    add_and_verify_task(&counter_task_config, &counter_task_id);
    add_and_verify_task(&blink_task_config, &blink_task_id);
    add_and_verify_task(&breathe_task_config, &breathe_task_id);

    add_and_verify_task(&led_pattern_task_config, &led_pattern_task_id);
    task_entry_t* led = get_task_by_id(led_pattern_task_id);
    if(led != NULL) led->state = TASK_BLOCKED;

    add_and_verify_task(&supervisor_task_config, &supervisor_task_id);
    task_entry_t* t = get_task_by_id(supervisor_task_id);
    if(t != NULL) t->state = TASK_BLOCKED;
    task_id_t idle_id;

    add_and_verify_task(&error_led_task_config, &error_led_task_id);
    t = get_task_by_id(error_led_task_id);
    if(t != NULL) t->state = TASK_BLOCKED;

    add_task(&idle_task_config, &idle_id);

    scheduler_start();

    //should never reach this point.
    while(1){
        PORTB |= COUNTER_MASK;
        PORTB |= (1U << ERROR_LED);
        PORTB |= (1U << BLINK_LED);
        PORTD |= (1U << PWM_LED);
        PORTD &= ~(1U << POWER_ON_LED);
    }
}   