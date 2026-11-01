#include "task.h"
#include <avr/io.h>
#include "time.h"
#include <stddef.h>
#include "mutex.h"
#include "scheduler.h"

static uint8_t task_count = 0;
static uint8_t next_task_id = 1;
static task_entry_t tasks[MAX_TASKS];
#define STACK_CANARY 0xA5
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE];
static uint8_t last_scheduled_index = 0;

task_entry_t *current_running_task = NULL;
task_entry_t *next_task_to_run = NULL;

scheduler_status_t add_task(const task_config_t *config, task_id_t *id)
{
    if (config == 0 ||
        config->function == 0)
    {
        return SCHEDULER_ERR_INVALID_ARGUMENT;
    }
    if (id == 0)
    {
        return SCHEDULER_ERR_INVALID_ARGUMENT;
    }
     for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].id != TASK_ID_INVALID && tasks[i].function == config->function)
        {
            return SCHEDULER_ERR_INVALID_ARGUMENT;
        }
    }
    if (task_count >= MAX_TASKS)
    {
        return SCHEDULER_ERR_FULL;
    }
    if (next_task_id == TASK_ID_INVALID)
    {
        return SCHEDULER_ERR_FULL;
    }
    task_entry_t *task = &tasks[next_task_id - 1];
    task_count++;
    task->id = next_task_id;
    *id = next_task_id;
    update_next_task_id(); // conflict test point.
    task->function = config->function;
    task->cleanup = config->cleanup;
    task->interval_ms = config->interval_ms;
    task->state = TASK_WAITING;
    task->priority = config->priority;
    task->port = config->port;

    task->next_run = timer_get_hardware_ticks() + MS_TO_TICKS(task->interval_ms);

    task_stacks[next_task_id - 1][0] = STACK_CANARY;
    uint8_t *stk = &task_stacks[next_task_id - 1][TASK_STACK_SIZE - 1];

    uint16_t function_address = (uint16_t)task->function;
    *stk-- = (uint8_t)(function_address & 0xFF); // storing function address.
    *stk-- = (uint8_t)((function_address >> 8) & 0xFF);

    *stk-- = 0x00; // r0
    *stk-- = 0x80; // SREG MSB is the interrupt enable bit.
    *stk-- = 0x00; // r1, must be zero always.

    for (uint8_t i = 2; i <= 31; i++)
    { // pushing the values of r2 to r31 as 0.
        *stk-- = 0x00;
    }
    task->sp = stk;

    return SCHEDULER_OK;
}

void update_next_task_id(void)
{
    for (uint8_t candidate = 1; candidate <= MAX_TASKS; candidate++)
    {
        uint8_t in_use = 0;
        for (uint8_t i = 0; i < MAX_TASKS; i++)
        {
            if (tasks[i].id == candidate)
            {
                in_use = 1;
                break;
            }
        }
        if (!in_use)
        {
            next_task_id = candidate;
            return;
        }
    }
    next_task_id = TASK_ID_INVALID; // all IDs exhausted.
}

void delete_one_task(task_id_t to_remove)
{
    if (portb_mutex.locked && portb_mutex.owner == to_remove)
        mutex_unlock(&portb_mutex);
    if (portc_mutex.locked && portc_mutex.owner == to_remove)
        mutex_unlock(&portc_mutex);
    if (portd_mutex.locked && portd_mutex.owner == to_remove)
        mutex_unlock(&portd_mutex);
    portb_mutex.mask &= ~(1U << (to_remove - 1));
    portc_mutex.mask &= ~(1U << (to_remove - 1));
    portd_mutex.mask &= ~(1U << (to_remove - 1));

    for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].id == to_remove)
        {
            if (tasks[i].cleanup != NULL)
            {
                tasks[i].cleanup();
            }
            tasks[i].id = TASK_ID_INVALID;
            tasks[i].function = 0;
            tasks[i].cleanup = 0;
            tasks[i].interval_ms = 0;
            tasks[i].next_run = 0;
            tasks[i].state = TASK_BLOCKED;
            tasks[i].priority = 0;
            tasks[i].port = Z; // default to no port usage.
            task_count--;
            update_next_task_id(); // set next_task_id to the removed task ID.
            return;
        }
    }
}

uint8_t get_next_task_id(void)
{
    return next_task_id;
}

uint8_t get_task_count(void)
{
    return task_count;
}

void set_next_task_id(uint8_t id)
{
    next_task_id = id;
}
void set_task_count(uint8_t count)
{
    task_count = count;
}
task_entry_t *get_task_by_index(uint8_t index)
{
    if (index < MAX_TASKS)
    {
        return &tasks[index];
    }
    return NULL;
}

task_entry_t *get_task_by_id(uint8_t id)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].id == id)
        {
            return &tasks[i];
        }
    }
    return NULL;
}

uint8_t task_check_stack(task_id_t id)
{
    for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].id == id)
        {
            return (task_stacks[i][0] == STACK_CANARY);
        }
    }
    return 0;
}

void task_yield(void)
{
    update_ready_list();

    uint32_t earliest_wake = determine_wake_up_time();
    uint32_t now = timer_get_hardware_ticks();
    if ((int32_t)(earliest_wake - now) > 0)
    {
        uint16_t slice = (uint16_t)(earliest_wake - now);
        if (slice > 60000)
            slice = 60000;
        OCR1B = (uint16_t)(TCNT1 + slice);
        TIFR1 |= (1U << OCF1B);
        TIMSK1 |= (1U << OCIE1B);
    }
    task_entry_t *best_task = NULL;
    uint8_t highest_priority = 0;
    uint8_t best_index = last_scheduled_index;
    for (uint8_t count = 0; count < MAX_TASKS; count++)
    {
        uint8_t i = (count + last_scheduled_index + 1) % MAX_TASKS;
        task_entry_t *task = get_task_by_index(i);
        if (task == NULL || task->id == TASK_ID_INVALID || task->state != TASK_READY)
            continue;

        if (best_task == NULL || task->priority > highest_priority)
        {
            best_task = task;
            highest_priority = task->priority;
            best_index = i;
        }
    }
    if (best_task != NULL)
    {
        if(current_running_task != NULL && current_running_task->state == TASK_RUNNING){
            current_running_task->state = TASK_READY;
        }
        next_task_to_run = best_task;
        last_scheduled_index = best_index;
        next_task_to_run->state = TASK_RUNNING;
        if (next_task_to_run != current_running_task)
        {
            scheduler_context_switch();
        }
    }
}

void task_delay_ms(uint32_t ms)
{
    if (current_running_task == NULL)
        return;

    current_running_task->state = TASK_WAITING;
    current_running_task->next_run = timer_get_hardware_ticks() + MS_TO_TICKS(ms);

    task_yield();
}

void scheduler_start(void)
{
    task_entry_t *best_task = NULL;
    uint8_t highest_priority = 0;

    for (uint8_t i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].id != TASK_ID_INVALID && (tasks[i].state == TASK_READY || tasks[i].state == TASK_WAITING))
        {
            tasks[i].state = TASK_READY;
            if (best_task == NULL || tasks[i].priority > highest_priority)
            {
                highest_priority = tasks[i].priority;
                best_task = &tasks[i];
            }
        }
    }

    if (best_task != NULL)
    {
        current_running_task = NULL;
        next_task_to_run = best_task;
        next_task_to_run->state = TASK_RUNNING;
        scheduler_context_switch();
    }
}