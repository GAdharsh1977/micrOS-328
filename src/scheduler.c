#include <stdint.h>
#include "scheduler.h"
#include "time.h"
#include "task.h"
#include <avr/io.h>
#include <stddef.h>

void scheduler_init(scheduler_status_t *status)
{
    set_task_count(0);
    set_next_task_id(1); // conflict test point.
    *status = SCHEDULER_OK;
}

scheduler_status_t add_and_verify_task(const task_config_t *config, task_id_t *id)
{
    if (config->function == NULL)
    {
        return SCHEDULER_ERR_INVALID_ARGUMENT;
    }
    scheduler_status_t status = add_task(config, id);
    if (status != SCHEDULER_OK)
    {
        return status;
    }
    return SCHEDULER_OK;
}

void update_ready_list(){
    uint8_t i = 0;
    uint8_t task_count = get_task_count();
    for(i = 0; i < task_count; i++){
        task_entry_t* t = get_task_by_index(i);
        if(t->state != TASK_WAITING) continue;
        if((int32_t)(timer_get_hardware_ticks() - t->next_run) >= 0){
            t->state = TASK_READY;
        }
    }
}

uint32_t determine_wake_up_time(){
    uint32_t wake_up_time = 0xFFFFFFFF;
    uint8_t task_count = get_task_count();
    uint8_t waiting = 0;
    if(!task_count) return timer_get_hardware_ticks();

    for(uint8_t i = 0; i < task_count; i++){
        task_entry_t* t = get_task_by_index(i);
        if(t == NULL) continue;
        if(t->state == TASK_READY) return timer_get_hardware_ticks();
        else if(t->state == TASK_BLOCKED) continue;
        if(t->state == TASK_WAITING && (int64_t)(wake_up_time - t->next_run) > 0){
            wake_up_time = t->next_run;
            waiting = 1;
        }
    }
    if(!waiting){
        return timer_get_hardware_ticks();
    }
    return wake_up_time;
}
