#ifndef __AIR780E_AT_H
#define __AIR780E_AT_H

#include "at_interface.h"

#define AIR780E_EN_PORT GPIOA               /* GPIO端口组 */
#define AIR780E_EN_CLK RCC_APB2Periph_GPIOA /* GPIO端口时钟 */
#define AIR780E_EN_PIN GPIO_Pin_1           /* GPIO端口号 */

// 定义通知位
#define NOTIFY_KEY1_PRESS (1 << 0)    // 按键1按下通知
#define NOTIFY_KEY2_PRESS (1 << 1)    // 按键2按下通知
#define NOTIFY_SEND_DATA (1 << 2)     // 发送数据到服务端通知
#define NOTIFY_USART_RX (1 << 3)      // 从服务端收到数据通知
#define NOTIFY_START_AIR780E (1 << 4) // 连接服务端通知
#define NOTIFY_STOP_AIR780E (1 << 5)  // 断开服务端通知

/* 定义一个函数指针类型，用于接收串口透传数据时进行回调 */
typedef void (*recv_cb_t)(const char *, int);

/**
 * AIR780E 是否连接服务器成功
 *
 * @return 成功：1，未成功：0
 */
int air780e_is_connected_server(void);

/**
 * 发送数据
 *
 * @param data 要发送的数据
 * @param len 数据的长度
 */
void air780e_send_data(const char *data, int len);
/**
 * 停止 AIR780E
 */
void air780e_stop(void);
/**
 * 启动AIR780E
 * @param ip        服务器 IP
 * @param port      服务器端口
 * @param cb        接收数据的回调函数，当收到数据时调用
 * @return 执行结果，成功：pdPASS，失败：pdFAIL
 */
void air780e_start(const char *ip, int port, recv_cb_t cb);
/**
 * 初始化AIR780E的控制脚
 */
void air780e_init(void);

#endif
