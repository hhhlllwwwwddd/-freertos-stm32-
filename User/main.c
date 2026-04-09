#include "stm32f10x_conf.h"

#include "led.h"
#include "usart.h"
#include "system.h"
#include "i2c.h"
#include "OLED.h"
#include "at.h"
#include "bike.h"
#include "linked_queue.h"

TimerHandle_t timer_upload_data;
TimerHandle_t timer_handle_fan;

// 分支操作
// 主分支操作

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // 禁用中断
    portDISABLE_INTERRUPTS();
    // 打印发生溢出的任务名
    printf("\n%s overflow\n", pcTaskName);
    // 无限循环，防止继续执行
    while (1)
        ;
}

void test(void *pvParameters)
{
    while (1)
    {
        led_red_toggle();
        // os_mem_info();
        os_delay_s(5);
    }
}

// 测试git提交
int main(void)
{
    /* 配置NVIC为4号分组方式 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    led_init();
    log_init(LOG_DEBUG);

    bike_init();

    printf("FreeRTOS system init for GuoGuoBike\r\n");

    xTaskCreate(test, "test", 128, NULL, 3, NULL);

    bike_start();
    /* GPS接收，要在 bike task 启动后 */
    usart3_init();

    // 开启任务调度器
    vTaskStartScheduler();
    printf("FreeRTOS system failed!! NEVER BEEN HERE！\n");
}
