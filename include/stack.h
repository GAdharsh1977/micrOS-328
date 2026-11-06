#ifndef STACK_H
#define STACK_H

#include "task.h"

#define STACK_LAST_BYTE 0xA5
#define STACK_DETECTOR_SIZE 4U
#define STACK_LOWER_END 0xDEADBEEFUL
#define STACK_UPPER_END 0xBEEFDEADUL
#define REBOOT_LED_TIME 100U

typedef struct {
    volatile uint32_t lower_end;
    uint8_t stacks[MAX_TASKS][TASK_STACK_SIZE];
    volatile uint32_t upper_end;
} guarded_stack_t;

extern guarded_stack_t guarded_memory;
#define task_stacks (guarded_memory.stacks)

uint8_t task_stack_memory_intact(void);
uint8_t kernel_memory_intact(void);
void __attribute__((noreturn)) full_silicon_reboot(void);

uint8_t task_check_stack(task_id_t id);

void reboot_slot(uint8_t slot);
void handle_stack_fault(task_entry_t* culprit);

#endif