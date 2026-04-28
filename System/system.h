#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "stm32f10x_conf.h"
#include "stdio.h"
#include "FreeRTOS.h" // FreeRTOS ��ͷ�ļ���ʹ��FreeRTOS�������
#include "task.h"     // FreeRTOS �������ͷ�ļ�
#include "semphr.h"   // FreeRTOS �ź���ͷ�ļ�
#include "queue.h"    // FreeRTOS ����ͷ�ļ�
#include "timers.h"   // FreeRTOS ������ʱ��ͷ�ļ�

#include "log.h"

uint32_t os_millis(void);
uint32_t os_millisFromISR(void);

void os_delay(int n);
void os_delay_ms(uint32_t ms);
void os_delay_s(uint32_t s);
void os_task_yield(void);
void os_mem_info(void);
void *os_malloc(size_t size);
void os_free(void *ptr);
#endif
