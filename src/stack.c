#include "stack.h"
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include "functions.h"
#include "time.h"
#include <util/delay.h>

guarded_stack_t guarded_memory = {
    .lower_end = STACK_LOWER_END,
    .upper_end = STACK_UPPER_END
};

uint8_t task_stack_memory_intact(void)
{
    for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        for (uint8_t j = 0; j < STACK_DETECTOR_SIZE; j++)
        {
            if (task_stacks[i][j] != STACK_LAST_BYTE)
            {
                return 0; // stack overflow detected.
            }
        }
    }
    return 1;
}

uint8_t kernel_memory_intact(void)
{
    return (guarded_memory.lower_end == STACK_LOWER_END) && (guarded_memory.upper_end == STACK_UPPER_END);
}

static void hw_delay_ms(uint16_t ms){
    OCR1B = (uint16_t)(TCNT1 + MS_TO_TICKS(ms));
    TIFR1 = (1U << OCF1B);
    while((TIFR1 & (1U << OCF1B)) == 0){
        //wait for compare match
    }
    TIFR1 = (1U << OCF1B);
}

void __attribute__((noreturn)) full_silicon_reboot(void)
{
    cli();
    PORTD &= ~(1U << POWER_ON_LED);
    PORTB &= ~(1U << BLINK_LED);
    PORTB &= ~COUNTER_MASK;
    pwm_led_set_duty(0);
    uint8_t blink_count = 0;
    while (blink_count < 10) //blink for 1s.
    {
        PORTB ^= (1U << ERROR_LED);
        hw_delay_ms(REBOOT_LED_TIME);
        blink_count++;
    }
    wdt_enable(WDTO_15MS);
    while(1){
        //need this to make it a noreturn function.
    }
}

uint8_t task_check_stack(task_id_t id)
{
    for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        task_entry_t *task = get_task_by_index(i);
        if (task == NULL)
        {
            continue;
        }
        if (task->id == id)
        {
            for (uint8_t j = 0; j < STACK_DETECTOR_SIZE; j++)
            {
                if (task_stacks[i][j] != STACK_LAST_BYTE)
                {
                    return 0; // stack overflow detected.
                }
            }
            return 1;
        }
    }
    return 0; // task not found.
}

void reboot_slot(uint8_t slot)
{
    task_entry_t *task = get_task_by_index(slot);
    if (slot >= MAX_TASKS || task == NULL || task->id == TASK_ID_INVALID)
    {
        return;
    }
    if (task->cleanup != NULL)
    {
        task->cleanup();
    }
    for (uint8_t i = 0; i < STACK_DETECTOR_SIZE; i++)
    {
        task_stacks[slot][i] = STACK_LAST_BYTE;
    }
    uint8_t *stk = &task_stacks[slot][TASK_STACK_SIZE - 1];
    uint16_t function_address = (uint16_t)task->function;
    *stk-- = (uint8_t)(function_address & 0xFF);        // function address high byte.
    *stk-- = (uint8_t)((function_address >> 8) & 0xFF); // function address low byte.

    *stk-- = 0x00; // r0
    *stk-- = 0x80; // SREG
    *stk-- = 0x00; // r1

    for (uint8_t i = 2; i < 32; i++)
    {
        *stk-- = 0x00;
    }
    task->sp = stk;
    task->next_run = timer_get_hardware_ticks();
    task->state = TASK_READY;
}

void handle_stack_fault(task_entry_t *culprit)
{
    if (!kernel_memory_intact())
    {
        full_silicon_reboot();
    }
    culprit->state = TASK_FAULTED;
    if (portb_mutex.owner == culprit->id)
    {
        mutex_unlock(&portb_mutex);
    }
    if (portc_mutex.owner == culprit->id)
    {
        mutex_unlock(&portc_mutex);
    }
    if (portd_mutex.owner == culprit->id)
    {
        mutex_unlock(&portd_mutex);
    }

    for (uint8_t slot = 0; slot < MAX_TASKS; slot++)
    {
        task_entry_t *task = get_task_by_index(slot);
        if (task == NULL)
            continue;
        if (task->id != TASK_ID_INVALID && task->id != culprit->id)
        {
            if (!task_check_stack(task->id))
            {
                if (portb_mutex.owner == task->id)
                {
                    mutex_unlock(&portb_mutex);
                }
                if (portc_mutex.owner == task->id)
                {
                    mutex_unlock(&portc_mutex);
                }
                if (portd_mutex.owner == task->id)
                {
                    mutex_unlock(&portd_mutex);
                }
                task->state = TASK_COLLATERAL_DAMAGE;
                reboot_slot(slot);
            }
        }
    }
    trigger_error_pulse();
}
