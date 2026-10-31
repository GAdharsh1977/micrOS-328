#include "functions.h"
#include "task.h"
#include <avr/io.h>
#include "pwm.h"
#include "time.h"
#include "scheduler.h"
static uint8_t counter = 0;

task_id_t counter_task_id = TASK_ID_INVALID;
task_id_t blink_task_id = TASK_ID_INVALID;
task_id_t breathe_task_id = TASK_ID_INVALID;
task_id_t led_pattern_task_id = TASK_ID_INVALID;
task_id_t supervisor_task_id = TASK_ID_INVALID;
task_id_t error_led_task_id = TASK_ID_INVALID;

uint8_t brightness = 0;
int8_t brightness_step = BRIGHTNESS_STEP;

static uint8_t step = 0;

const uint8_t pattern[LED_PATTERN_STEPS] = {
    0x01, 0x0A, 0x04, 0x0F,
    0x02, 0x09, 0x00, 0x06,
    0x08, 0x03, 0x0C, 0x01,
    0x07, 0x00, 0x0A, 0x05};

void counter_task(void)
{
    mutex_t *m = determine_mutex(counter_task_id);
    while (1)
    {
        while (!mutex_lock(m, counter_task_id))
        {
            task_yield();
        }
        counter = (counter + 1) & 0x0F;
        PORTB = (PORTB & (uint8_t)~COUNTER_MASK) |
                (uint8_t)((counter & 0x0F) << COUNTER_BIT0);
        mutex_unlock(m);
        task_delay_ms(COUNTER_INTERVAL_MS);
    }
}

void counter_task_cleanup(void)
{
    PORTB &= (uint8_t)~COUNTER_MASK;
    counter = 0;
}

void blink_task(void)
{
    mutex_t *m = determine_mutex(blink_task_id);
    while (1)
    {
        while (!mutex_lock(m, blink_task_id))
        {
            task_yield();
        }

        PORTB ^= (1U << BLINK_LED);
        mutex_unlock(m);
        task_delay_ms(BLINK_INTERVAL_MS);
    }
}

void blink_task_cleanup(void)
{
    PORTB &= (uint8_t)~(1U << BLINK_LED);
}

void led_breathe_task(void)
{
    mutex_t *m = determine_mutex(breathe_task_id);
    while (1)
    {
        while (!mutex_lock(m, breathe_task_id))
        {
            task_yield();
        }
        int16_t next = (int16_t)brightness + (int16_t)brightness_step;

        if (next >= 255)
        {
            brightness = 255;
            brightness_step = -BRIGHTNESS_STEP;
        }
        else if (next <= 0)
        {
            brightness = 0;
            brightness_step = BRIGHTNESS_STEP;
        }
        else
        {
            brightness = (uint8_t)next;
        }
        pwm_led_set_duty(brightness);
        mutex_unlock(m);
        task_delay_ms(BRIGHTNESS_UPDATE_MS);
    }
}

void led_breathe_task_cleanup(void)
{
    pwm_led_set_duty(0);
    brightness = 0;
    brightness_step = BRIGHTNESS_STEP;
}

void led_pattern_task(void)
{
    task_entry_t *t = get_task_by_id(led_pattern_task_id);
    mutex_t *m = determine_mutex(led_pattern_task_id);

    while (1)
    {
        if (m != NULL && mutex_lock(m, led_pattern_task_id))
        {
            for (step = 0; step < LED_PATTERN_STEPS; step++)
            {
                PORTB = (PORTB & (uint8_t)~COUNTER_MASK) |
                        (uint8_t)(pattern[step] << COUNTER_BIT0);

                task_delay_ms(LED_PATTERN_STEP_MS);
            }

            PORTB &= (uint8_t)~COUNTER_MASK;
            step = 0;
            mutex_unlock(m);

            if (t != NULL)
            {
                t->state = TASK_BLOCKED;
            }
            task_yield();
        }
        else
        {
            task_yield();
        }
    }
}

void led_pattern_cleanup(void)
{
    PORTB &= (uint8_t)~COUNTER_MASK;
    step = 0;
}

void idle_task(void)
{
    while (1)
    {
        uint32_t time = determine_wake_up_time();
        sleep_until(time);
        task_yield();
    }
}

void supervisor_task(void)
{
    task_entry_t *self = get_task_by_id(supervisor_task_id);
    while (1)
    {
        if (trigger_led_pattern_requested == 1)
        {
            trigger_led_pattern_requested = 0;
            task_entry_t *t = get_task_by_id(led_pattern_task_id);
            if (t != NULL && t->state == TASK_BLOCKED)
            {
                t->next_run = timer_get_hardware_ticks();
                t->state = TASK_READY;
            }
        }
        if (disable_breathing_requested == 2)
        {
            disable_breathing_requested = 0;
            if (add_and_verify_task(&breathe_task_config, &breathe_task_id) != SCHEDULER_OK)
            {
                trigger_error_pulse();
            }
        }
        if (self != NULL)
            self->state = TASK_BLOCKED;
        task_yield();
    }
}

void error_led_task(void)
{
    mutex_t *m = determine_mutex(error_led_task_id);
    task_entry_t *self = get_task_by_id(error_led_task_id);
    while (1)
    {
        while (m != NULL && !mutex_lock(m, error_led_task_id))
        {
            task_yield();
        }
        PORTB |= (1U << ERROR_LED);
        mutex_unlock(m);

        task_delay_ms(ERROR_LED_TIME);

        while (m != NULL && !mutex_lock(m, error_led_task_id))
        {
            task_yield();
        }
        PORTB &= ~(1U << ERROR_LED);
        mutex_unlock(m);

        if (self != NULL)
        {
            self->state = TASK_BLOCKED;
        }
        task_yield();
    }
}

void trigger_error_pulse(void)
{
    task_entry_t *err_task = get_task_by_id(error_led_task_id);
    if (err_task != NULL)
    {
        err_task->next_run = timer_get_hardware_ticks();
        err_task->state = TASK_READY;
        task_yield();
    }
}

const task_config_t counter_task_config = {
    .function = counter_task,
    .cleanup = counter_task_cleanup,
    .interval_ms = COUNTER_INTERVAL_MS,
    .priority = 2,
    .port = B};

const task_config_t blink_task_config = {
    .function = blink_task,
    .cleanup = blink_task_cleanup,
    .interval_ms = BLINK_INTERVAL_MS,
    .priority = 2,
    .port = B};

const task_config_t breathe_task_config = {
    .function = led_breathe_task,
    .cleanup = led_breathe_task_cleanup,
    .interval_ms = BRIGHTNESS_UPDATE_MS,
    .priority = 1,
    .port = D};

const task_config_t led_pattern_task_config = {
    .function = led_pattern_task,
    .cleanup = led_pattern_cleanup,
    .interval_ms = LED_PATTERN_STEP_MS,
    .priority = 2,
    .port = B};

const task_config_t idle_task_config = {
    .function = idle_task,
    .cleanup = NULL,
    .interval_ms = 0,
    .priority = 0,
    .port = Z};

const task_config_t supervisor_task_config = {
    .function = supervisor_task,
    .cleanup = NULL,
    .interval_ms = SUPERVISOR_TASK_MS,
    .priority = 5,
    .port = Z};

const task_config_t error_led_task_config = {
    .function = error_led_task,
    .cleanup = NULL,
    .interval_ms = ERROR_LED_TIME,
    .priority = 3,
    .port = B};