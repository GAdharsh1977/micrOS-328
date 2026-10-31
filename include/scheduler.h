#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "task.h"

void scheduler_init(scheduler_status_t *status);

scheduler_status_t add_and_verify_task(const task_config_t *config, task_id_t *id);

void update_ready_list();

uint32_t determine_wake_up_time();

#endif
