#include <stddef.h>
#include <stdint.h>

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include "task.h"

task_stack_t stack1[128];
void task1(void* param) {
    (void)param;
    while (1) {
        task_delay(500);
        GPIOC->ODR ^= GPIO_Pin_13;
    }
}

task_stack_t stack2[128];
void task2(void* param) {
    (void)param;
    while (1) {
        task_delay(5);
        GPIOC->ODR ^= GPIO_Pin_14;
    }
}

int main() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_Init(GPIOC, &(GPIO_InitTypeDef){
                         .GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14,
                         .GPIO_Mode = GPIO_Mode_Out_PP,
                         .GPIO_Speed = GPIO_Speed_2MHz,
                     });

    task_create(task1, stack1, 128, 1, NULL);
    task_create(task2, stack2, 128, 1, NULL);
    scheduler_start();
    // We should never get here.
    while (1)
        ;
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line) {
    (void)file;
    (void)line;
    while (1)
        ;
}

#endif