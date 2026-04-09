#ifndef __LINKED_QUEUE_H
#define __LINKED_QUEUE_H

#include "stm32f10x_conf.h"
#include "system.h"
#include "semphr.h"

/* 注意！此队列不适用于中断中调用 */

#define OK 0          /* 成功 */
#define ERROR -1      /* 失败 */

/* 定义一个通用的数据类型 */
typedef char* data_type_t;
/* 链式队列的节点结构体 */
typedef struct queue_node {
    data_type_t data;             // 数据域，使用通用数据类型
    struct queue_node *next;      // 指针域，指向下一个节点 
} queue_node_t;
/* 链式队列头结点结构体，没有数据域 */
typedef struct {
    queue_node_t *front;        // 队头指针
    queue_node_t *rear;         // 队尾指针
    SemaphoreHandle_t mutex;    // 互斥锁
} queue_t;

/* 判空操作，如果是空返回1，非空返回0 */
int is_empty(queue_t *queue);
/* 入队操作 */
int enqueue(queue_t *queue, data_type_t value);
/* 出队操作 */
int dequeue(queue_t *queue, data_type_t *value);
/* 查看队列顶元素,仅返回队头元素的值，而不修改队列的状态  */
int queue_head(queue_t *queue, data_type_t *value);
/* 初始化队列 */
void init_queue(queue_t *queue);

/* 选择测试整型还是字符串 */
//#define __QUEUE_TEST_TYPE_INT__
#define __QUEUE_TEST_TYPE_STRING__

#ifdef __QUEUE_TEST_TYPE_INT__
int queue_test_type_int(void);
#endif

#ifdef __QUEUE_TEST_TYPE_STRING__
int queue_test_type_string(void);
#endif

#endif
