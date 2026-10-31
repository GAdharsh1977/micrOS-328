#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t task_id_t;
#define TASK_ID_INVALID 0x00
#define MAX_TASKS 0x08
#define TASK_STACK_SIZE 96U

typedef enum
{
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_WAITING
} task_state_t;

typedef enum
{
    B,
    C,
    D,
    Z // invalid port
} port_t;

typedef enum
{
    SCHEDULER_OK = 0,
    SCHEDULER_ERR_INVALID_TASK,
    SCHEDULER_ERR_FULL,
    SCHEDULER_ERR_INVALID_ID,
    SCHEDULER_ERR_INVALID_ARGUMENT
} scheduler_status_t;

typedef struct
{
    void (*function)(void);
    void (*cleanup)(void);
    uint32_t interval_ms;
    uint8_t priority;
    port_t port;
} task_config_t;

typedef struct
{
    uint8_t *sp;
    void (*function)(void);
    void (*cleanup)(void);
    uint8_t id;
    uint32_t interval_ms;
    uint32_t next_run;
    task_state_t state;
    uint8_t priority;
    port_t port;
} task_entry_t;

extern task_entry_t* current_running_task;
extern task_entry_t* next_task_to_run;

scheduler_status_t add_task(const task_config_t *config, task_id_t *id);
void delete_one_task(task_id_t to_remove);

void update_next_task_id(void);

uint8_t get_next_task_id(void);
uint8_t get_task_count(void);

void set_next_task_id(uint8_t id);
void set_task_count(uint8_t count);

task_entry_t* get_task_by_index(uint8_t index);
task_entry_t* get_task_by_id(uint8_t id);

void task_yield(void);
void task_delay_ms(uint32_t ms);
void scheduler_start(void);
void scheduler_context_switch(void);

#endif