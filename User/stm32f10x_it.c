#include "stm32f10x_conf.h"
#include "string.h"
#include "led.h"
#include "key.h"
#include "system.h"
#include "at_air780e.h"
#include "bike.h"
#include "usart.h"
#include "ringbuffer.h"

extern TaskHandle_t at_task_handle;

volatile uint32_t key1_last_interrupt_time = 0;
volatile uint32_t key2_last_interrupt_time = 0;

/* 外部中断EXTI0的中断处理函数
 * External Interrupt 0 Interrupt Request Handler
 */
void EXTI0_IRQHandler(void)
{
    BaseType_t taskWoken = pdFALSE;
    /* 判断是 EXTI_Line0 产生中断 */
    if (EXTI_GetITStatus(KEY1_EXTI_LINE) != RESET)
    {
        if (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == 0)
        {
            xTaskNotifyFromISR(
                bike_task_handle,
                NOTIFY_KEY1_PRESS,
                eSetBits, // 设置标志位，不覆盖
                &taskWoken);
            portYIELD_FROM_ISR(taskWoken);
        }
        /* 清除中断标志位 */
        EXTI_ClearITPendingBit(KEY1_EXTI_LINE);
    }
}
/* 外部中断 EXTI15_10 的中断处理函数
 * External Interrupt 0 Interrupt Request Handler
 */
void EXTI15_10_IRQHandler(void)
{
    BaseType_t taskWoken = pdFALSE;
    /* 判断是 EXTI_Line14 产生中断
     * 该函数会检测 EXTI中【中断等待寄存器，Pending register】对应位是否置位
     */
    if (EXTI_GetITStatus(KEY2_EXTI_LINE) != RESET)
    {
        if (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == 0)
        {
            xTaskNotifyFromISR(
                bike_task_handle,
                NOTIFY_KEY2_PRESS,
                eSetBits, // 设置标志位，不覆盖
                &taskWoken);
            portYIELD_FROM_ISR(taskWoken);
        }
        /* 清除【中断等待寄存器，Pending register】中的中断标志位 */
        EXTI_ClearITPendingBit(KEY2_EXTI_LINE);
    }
}
/**
 * @brief  处理串口收到的命令
 * @param  串口每次收到的字节
 * @retval 无
 */
void cmd_revevied(uint8_t rx_data)
{
#define RX_BUFFER_SIZE 16
    static char rx_buffer[RX_BUFFER_SIZE];
    static uint16_t rx_index = 0;
    BaseType_t taskWoken = pdFALSE;

    rx_buffer[rx_index++] = rx_data;

    // 检测到换行符，表示一条命令
    if (rx_data == '\n')
    {
        rx_buffer[rx_index] = '\0'; // 确保字符串以NULL结尾

        // 检查是否是读取数据的命令
        if (strncmp(rx_buffer, "get data", 8) == 0)
        {
            // 通知任务进行数据处理
            xTaskNotifyFromISR(bike_task_handle, NOTIFY_DATA_LIST,
                               eSetBits, &taskWoken);
        }
        else
        {
            bike_data_exprot_request(rx_buffer);
            // 通知任务进行数据处理
            xTaskNotifyFromISR(bike_task_handle, NOTIFY_DATA_EXPORT,
                               eSetBits, &taskWoken);
        }

        portYIELD_FROM_ISR(taskWoken);

        rx_index = 0; // 复位索引，准备接收下一条句子
    }
}
void USART1_IRQHandler(void)
{
    uint8_t rx_data;

    /* 判断 USART1 是否产生接受中断 */
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        rx_data = USART_ReceiveData(USART1);
        cmd_revevied(rx_data);
        /* 清除 USART1 产生的接受中断 */
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

extern void at_receive_byte(char byte);

void USART2_IRQHandler(void)
{
    uint8_t rx_data;
    BaseType_t taskWoken = pdFALSE;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        rx_data = USART_ReceiveData(USART2);

        at_receive_byte(rx_data);

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }

    // IDLE中断：串口空闲（接收一帧完成）
    if (USART_GetITStatus(USART2, USART_IT_IDLE) == SET)
    {
        // 非常重要！读SR后再读DR清除IDLE标志
        volatile uint32_t temp;
        temp = USART2->SR;
        temp = USART2->DR;
        (void)temp; // 防止编译器警告

        // 通知任务进行数据处理
        xTaskNotifyFromISR(
            at_task_handle,
            NOTIFY_USART_RX,
            eSetBits, // 设置标志位
            &taskWoken);
        portYIELD_FROM_ISR(taskWoken);

        USART_ClearITPendingBit(USART2, USART_IT_IDLE);
    }
}
extern void gps_data_revevied(uint8_t rx_data);
void USART3_IRQHandler(void)
{
    uint8_t rx_data;
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        rx_data = USART_ReceiveData(USART3);
        // 处理gps数据，通知任务进行数据处理
        gps_data_revevied(rx_data);

        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}
