#include <stdlib.h>

#include "stm32f10x.h"

#include "MiniRTOS_config.h"
#include "task.h"

static task_handler_t *task_now, *task_head, *task_last;

static task_stack_t idle_task_stack[IDLE_TASK_STACK_SIZE];
static uint32_t idle_task_PSP =
    (uint32_t)(idle_task_stack + IDLE_TASK_STACK_SIZE - 16);
// a fake task handler
static task_handler_t* idle_task_handler = (void*)&idle_task_PSP;
static void idle_task() {
    while (1) {
        __DSB();
        __WFI();
    }
}

/**
 * If a task returns, it will jump here.
 */
static void task_exit() {
    while (1)
        ;
}

/**
 * @param task Function to be executed.
 * @param stack At least 16 words (64 bytes). must be Double Word Aligned.
 * @param size The number of words in the stack, must >= 16.
 * @param priority Priority of task, must >= 1.
 * @param param Parameter passed to the task.
 * @return handler of the task. NULL if failed.
 */
task_handler_t* task_create(void (*task)(void*),
                            uint32_t* stack,
                            uint32_t size,
                            uint8_t priority,
                            void* param) {
    __disable_irq();
    task_handler_t* handler = malloc(sizeof(task_handler_t));
    if (handler == NULL) {
        __enable_irq();
        return NULL;
    }
    stack[size - 1] = 0x01000000;           // xPSR
    stack[size - 2] = (uint32_t)task;       // PC
    stack[size - 3] = (uint32_t)task_exit;  // LR
    // stack[size - 4] = 0x00000000;         // R12
    // stack[size - 5] = 0x00000000;         // R3
    // stack[size - 6] = 0x00000000;         // R2
    // stack[size - 7] = 0x00000000;         // R1
    stack[size - 8] = (uint32_t)param;  // R0, the parameter passed to the task.
    handler->PSP = stack + size - 16;
    handler->timeout = 0;
    handler->state = ready;
    handler->priority = priority;
    if (task_head == 0) {
        task_head = handler;
    } else {
        handler->next = task_head->next;
    }
    task_head->next = handler;
    __enable_irq();
    return handler;
}

/**
 * @brief delay a task
 * @note delay time is imprecise
 * @param ms milliseconds to delay. 0 stands for infinity
 */
void task_delay(uint32_t ms) {
    __disable_irq();
    task_now->state = suspend;
    task_now->timeout = ms;
    __enable_irq();
    volatile uint8_t* status = &(task_now->state);
    while (*status == suspend) {
        __DSB();
        __WFE();
    }
}

/**
 * Make sure that at least one task has been created.
 * This function never return.
 */
__attribute__((naked, noreturn)) void scheduler_start() {
    NVIC_SetPriority(PendSV_IRQn, 0xF);
    // create idle task
    idle_task_stack[IDLE_TASK_STACK_SIZE - 1] = 0x01000000;           // xPSR
    idle_task_stack[IDLE_TASK_STACK_SIZE - 2] = (uint32_t)idle_task;  // PC
    task_now = task_head;
    SysTick->LOAD = 9000;  // 1ms
    SysTick->VAL = 0;
    SysTick->CTRL = 0x03;
    __ASM("ldr r0, %0;" ::"m"(task_now->PSP));
    __ASM(
        "msr psp, r0;"
#ifdef TASK_PRIVILEGED
        "ldr r0, =0x02;"  // run tasks at privileged level
#else
        "ldr r0, =0x03;"  // run tasks at unprivileged level
#endif
        "msr control, r0;"  // use PSP
        "isb;"
        "pop {r4-r11};"
        "pop {r0-r3, r12, lr};"
        "pop {r4, r5};"  // r4 = PC, r5 = xPSR
        "msr xPSR, r5;"
        "mov pc, r4");  // jump to task 1
    // no return
}

void SysTick_Handler() {
    task_last = task_now;
    uint8_t max_priority = 0;
    task_handler_t* stop = task_head;
    do {
        if (task_head->state == suspend) {
            if (task_head->timeout > 0) {
                task_head->timeout--;
                if (task_head->timeout == 0) {
                    task_head->state = ready;
                }
            }
        }
        if (task_head->state == ready) {
            if (task_head->priority > max_priority) {
                max_priority = task_head->priority;
                task_now = task_head;
            }
        }
        task_head = task_head->next;
    } while (task_head != stop);

    if (max_priority > 0) {
        // next task found, move task_head
        task_head = task_head->next;
    } else {
        // not found, keep task_head still
        task_now = idle_task_handler;
    }

    if (task_last != task_now) {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;  // set PendSV
        __DSB();
        __ISB();
    }
}

__attribute__((naked)) void PendSV_Handler() {
    __ASM(
        "mrs r0, psp;"
        "stmdb r0!,{r4-r11};");  // push r4-r11 on PSP
    __ASM("str r0, %0" : "=m"(task_last->PSP));
    __ASM("ldr r0, %0;" ::"m"(task_now->PSP));
    __ASM(
        "ldmia r0!, {r4-r11};"  // pop r4-r11 from PSP
        "msr psp, r0;"
        "bx lr;");
}
