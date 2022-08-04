#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define task_stack_t _Alignas(uint64_t) uint32_t

typedef enum task_state {
    ready,
    suspend,
} task_state_t;

typedef struct task_handler {
    uint32_t* PSP;
    struct task_handler* next;
    uint32_t timeout;  // suspend timeout, 0 stands for infinity
    uint8_t state;     // task_state_t
    uint8_t priority;  // must > 0
} task_handler_t;

task_handler_t* task_create(void (*task)(void*),
                            uint32_t* stack,
                            uint32_t size,
                            uint8_t priority,
                            void* param);
void task_delay(uint32_t ms);
void scheduler_start();

#endif