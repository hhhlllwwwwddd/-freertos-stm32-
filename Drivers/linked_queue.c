#include <stdio.h>
#include <stdlib.h>
#include "linked_queue.h"

/* 初始化队列 */
void init_queue(queue_t *queue)
{
    queue->front = queue->rear = NULL;
    queue->mutex = xSemaphoreCreateMutex(); // 创建互斥锁
    if(queue->mutex == NULL) {
        Log_e("init queue failed!");
        while (1);
    }
}
/* 判空操作，如果是空返回1，非空返回0 */
int is_empty(queue_t *queue)
{
    return queue->front == NULL;
}
/* 判满操作，注意！链式队列不需要此函数
 * 因为它的空间是动态分配的，只要还有内存，理论上就不会“满”
 * 这里函数原型不删除，供大家参考
 */
/*
int is_full(queue_t *queue)
{
    
}
*/
/* 入队操作 */
int enqueue(queue_t *queue, data_type_t value)
{
    queue_node_t *new_node = (queue_node_t*)os_malloc(sizeof(queue_node_t));
    if (!new_node) {
        return ERROR; // 内存分配失败
    }
    new_node->data = value;
    new_node->next = NULL;
    
    xSemaphoreTake(queue->mutex, portMAX_DELAY);

    if (is_empty(queue)) {
        queue->front = queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    xSemaphoreGive(queue->mutex);
    return OK;
}

/* 出队操作 */
int dequeue(queue_t *queue, data_type_t *value)
{
    queue_node_t *del_node;

    xSemaphoreTake(queue->mutex, portMAX_DELAY);

    if (is_empty(queue)) {
        xSemaphoreGive(queue->mutex);
        return ERROR; // 队列为空
    }
    del_node = queue->front;
    *value = del_node->data;
    queue->front = queue->front->next;
    if (queue->front == NULL) { // 如果是删除最后一个有效节点
        queue->rear = NULL;     // 应当把rear也置为空指针
    }
    
    xSemaphoreGive(queue->mutex);
    os_free(del_node);
    return OK;
}
/* 查看队列顶元素,仅返回队头元素的值，而不修改队列的状态  */
int queue_head(queue_t *queue, data_type_t *value)
{
    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    
    if (is_empty(queue)) {
        xSemaphoreGive(queue->mutex);
        return ERROR;
    }
    *value = queue->front->data;

    xSemaphoreGive(queue->mutex);
    return OK;
}
#ifdef __QUEUE_TEST_TYPE_INT__
/* 测试代码 */
int queue_test_type_int(void)
{
    queue_t queue;
    int value;

    /* 初始化队列 */
    init_queue(&queue);

    if (is_empty(&queue)) {
        printf("Quueue is empty\n\n");
    }
    else {
        printf("Queue not empty\n\n");
    }
    /* 增加一些元素 */
    printf("add [%d] into queue\n", 10);
    enqueue(&queue, 10);
    printf("add [%d] into queue\n", 20);
    enqueue(&queue, 20);
    printf("add [%d] into queue\n", 30);
    enqueue(&queue, 30);
    printf("add [%d] into queue\n", 40);
    enqueue(&queue, 40);

    queue_head(&queue, &value);
    printf("front value of queue is [%d]\n", value);

    while (!is_empty(&queue)) {
        dequeue(&queue, &value);
        printf("dequeue value [%d]\n", value);
    }

    return 0;
}
#endif

#ifdef __QUEUE_TEST_TYPE_STRING__
/* 假设每个字符串最大长度为 32 */
#define STR_BUF_LEN 32

/* 测试代码 */
int queue_test_type_string(void)
{
    queue_t queue;
    char* value;
    int i;

    /* 初始化队列 */
    init_queue(&queue);

    if (is_empty(&queue)) {
        printf("Queue is empty\n\n");
    } else {
        printf("Queue not empty\n\n");
    }

    /* 增加一些字符串元素 */
    const char *test_data[] = {"apple", "banana", "cherry", "date"};
    for (i = 0; i < sizeof(test_data)/sizeof(test_data[0]); i++) {
        char *str = (char *)os_malloc(STR_BUF_LEN);
        if (!str) {
            printf("Memory allocation failed\n");
            return -1;
        }
        snprintf(str, STR_BUF_LEN, "%s", test_data[i]);
        printf("add [%s] into queue\n", str);
        enqueue(&queue, str);
    }

    if (queue_head(&queue, &value) == OK) {
        printf("front value of queue is [%s]\n", value);
    }

    while (!is_empty(&queue)) {
        if (dequeue(&queue, &value) == OK) {
            printf("dequeue value [%s]\n", value);
            os_free(value);  // 释放字符串内存
        }
    }

    return 0;
}
#endif
