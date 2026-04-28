#include "at_air780e.h"
#include "linked_queue.h"
#include "bike.h"

#define AT_COMMAND_MAX_LEN 128

typedef enum
{
    AIR780E_ECHO_OFF = 0, // 对应 ATE0，关闭回显
    AIR780E_ECHO_ON = 1   // 对应 ATE1，开启回显
} AIR780E_ECHO_MODE;

const char *RESPONSE_OK = "OK\r\n";
const char *RESPONSE_ERROR = "ERROR";
const char *RESPONSE_FAIL = "FAIL";

TaskHandle_t at_task_handle = NULL; // AT 任务句柄

/* 如下这些变量，可以封装成一个结构体 */
static char air780e_ip[16];
static uint16_t air780e_port;

static char recv_buf[512];
static int recv_data_len = 0;

recv_cb_t recv_cb; /* 函数指针 */

static queue_t cmd_queue; // 命令队列，用于存储要发送的字符串数据

#define AIR780E_CONNECTED_SERVER 1
#define AIR780E_NOT_CONNECTED_SERVER 0
static int _air780e_is_connected_server = AIR780E_NOT_CONNECTED_SERVER;

/**
 * 测试AIR780E是否正常上电、准备就绪
 *
 * @return 执行结果
 */
AT_RESULT air780e_ready(void)
{
    const char *cmd = "AT\r";
    return at_execute((char *)cmd, RESPONSE_OK, RESPONSE_ERROR, NULL, 0);
}
/**
 * 开启或关闭 AT 指令回显
 *
 * @param enable 1 开启，0 关闭
 * @return 执行结果
 */
AT_RESULT air780e_set_echo(AIR780E_ECHO_MODE enable)
{
    const char *cmd = enable ? "ATE1\r" : "ATE0\r";
    return at_execute((char *)cmd, RESPONSE_OK, RESPONSE_ERROR, NULL, 0);
}
/**
 * 测试AIR780E是否SIM卡解锁
 *
 * @return 执行结果
 */
AT_RESULT air780e_sim_unlock(void)
{
    char status[10];
    char response[AT_COMMAND_MAX_LEN];
    AT_RESULT result;

    result = at_execute("AT+CPIN?\r", RESPONSE_OK, RESPONSE_ERROR, response, sizeof(response));
    if (result != AT_OK)
    {
        Log_e("Failed to execute AT+CPIN\n");
        return result;
    }

    // 解析完整响应
    Log_d("AT+CPIN Response: %s\n", response);
    // 使用 sscanf 提取 STATUS 值
    char *pos = strstr(response, "+CPIN:");
    if (pos)
    {
        if (sscanf(pos, "+CPIN: %s", status) == 1)
        {
            if (strcmp(status, "READY") == 0)
            {
                return AT_OK;
            }
        }
        else
        {
            Log_e("AT+CPIN return wrong status:%s.\n", status);
        }
    }

    return AT_FAILED;
}
/**
 * 测试AIR780E是否已注册到运营商网络
 *
 * @return 执行结果
 */
AT_RESULT air780e_register(void)
{
    char status[10];
    char response[AT_COMMAND_MAX_LEN];
    AT_RESULT result;

    result = at_execute("AT+CREG?\r", RESPONSE_OK, RESPONSE_ERROR, response, sizeof(response));
    if (result != AT_OK)
    {
        Log_e("Failed to execute AT+CREG\n");
        return result;
    }

    // 解析完整响应
    Log_d("AT+CREG Response: %s\n", response);
    // 使用 sscanf 提取 STATUS 值
    char *pos = strstr(response, "+CREG:");
    if (pos)
    {
        if (sscanf(pos, "+CREG: %s", status) == 1)
        {
            if (strcmp(status, "0,1") == 0)
            {
                return AT_OK;
            }
        }
        else
        {
            Log_e("AT+CREG return wrong status:%s.\n", status);
        }
    }

    return AT_FAILED;
}
/**
 * 测试AIR780E是否已附着到GPRS网络
 *
 * @return 执行结果
 */
AT_RESULT air780e_attached(void)
{
    char status[10];
    char response[AT_COMMAND_MAX_LEN];
    AT_RESULT result;

    result = at_execute("AT+CGATT?\r", RESPONSE_OK, RESPONSE_ERROR, response, sizeof(response));
    if (result != AT_OK)
    {
        Log_e("Failed to execute AT+CGATT\n");
        return result;
    }

    // 解析完整响应
    Log_d("AT+CGATT Response: %s\n", response);
    // 使用 sscanf 提取 STATUS 值
    char *pos = strstr(response, "+CGATT: ");
    if (pos)
    {
        if (sscanf(pos, "+CGATT: %s", status) == 1)
        {
            if (strcmp(status, "1") == 0)
            {
                return AT_OK;
            }
        }
        else
        {
            Log_e("AT+CGATT return wrong status:%s.\n", status);
        }
    }

    return AT_FAILED;
}
/**
 * 设置APN
 *
 * @return 执行结果
 */
AT_RESULT air780e_set_apn(void)
{
    return at_execute("AT+CSTT\r", RESPONSE_OK, RESPONSE_ERROR, NULL, 0);
}
/**
 * 激活PDP上下文
 *
 * @return 执行结果
 */
AT_RESULT air780e_activate_pdp(void)
{
    return at_execute("AT+CIICR\r", RESPONSE_OK, RESPONSE_ERROR, NULL, 0);
}
/**
 * 关闭PDP上下文
 *
 * @return 执行结果
 */
AT_RESULT air780e_shut_pdp(void)
{
    return at_execute("AT+CIPSHUT\r", "SHUT OK\r\n", RESPONSE_ERROR, NULL, 0);
}
/**
 * 设置透传模式
 * @return 执行结果
 */
AT_RESULT air780e_set_passthrough_mode(void)
{
    AT_RESULT result;

    result = at_execute("AT+CIPMODE=1\r", RESPONSE_OK, RESPONSE_ERROR, NULL, 0);
    if (result != AT_OK)
    {
        Log_e("Failed to execute AT+CIPMODE\n");
        return result;
    }
    return AT_OK;
}
/**
 * 退出透传模式
 *
 * 第一个 + 之前需要 1000ms 的间隔
 * 最后一个 + 之后需要 500ms 的间隔
 * 三个 + 之间的间隔不能超过 500ms
 */
void air780e_exit_passthrough_mode(void)
{
    os_delay_s(1);
    at_send_cmd("+++"); // 直接发送退出透传模式命令
    os_delay_ms(500);
}
/**
 * 启动连接
 *
 * @param type 连接类型 ("TCP" 或 "UDP")
 * @param ip 目标服务器的 IP 地址
 * @param port 目标端口号
 * @return 执行结果
 */
AT_RESULT air780e_connect_server(const char *type, const char *ip, int port)
{
    char cmd[AT_COMMAND_MAX_LEN];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%d\r", type, ip, port);
    return at_execute(cmd, "CONNECT\r\n", RESPONSE_ERROR, NULL, 0);
}
/**
 * 断开与服务器的连接
 *
 * @return 执行结果
 */
AT_RESULT air780e_disconnect_server()
{
    return at_execute("AT+CIPCLOSE\r", "CLOSE OK\r\n", RESPONSE_ERROR, NULL, 0);
}

/**
 * 发送数据
 *
 * @param data 要发送的数据
 * @param len 数据的长度
 */
void air780e_send_data(const char *data, int len)
{
    if (_air780e_is_connected_server == AIR780E_CONNECTED_SERVER)
    {
        char *str = (char *)os_malloc(len + 1);
        if (!str)
        {
            Log_e("Memory allocation failed\n");
            return;
        }
        strcpy(str, data);
        enqueue(&cmd_queue, str);             // 将要发送的字符串数据入比如串口发送数据入链式队列，等待发送任务处理
        Log_d("enqueue address [%p]\n", str); // remember to free!
        xTaskNotify(at_task_handle, NOTIFY_SEND_DATA, eSetBits);
    }
}
/**
 * 接收数据
 * @param buffer 缓冲区
 * @param size   缓冲区大小
 * @return 实际接收到的数据长度
 */
int air780e_recv_data(char *buffer, int size)
{
    return at_passthrough_receive(buffer, size);
}

/**
 * 重启AIR780E
 */
void air780e_reset(void)
{
    static int first_run = 1;
    // 第一次运行不打印，以免歧义
    first_run == 1 ? first_run = 0 : Log_e("\n***** Critical error occurred! AIR780E Reset *****\n");
    GPIO_SetBits(AIR780E_EN_PORT, AIR780E_EN_PIN);
    os_delay(12000); // 调度器还没启动，不能调用vTaskDelay
    GPIO_ResetBits(AIR780E_EN_PORT, AIR780E_EN_PIN);
}
/**
 * 返回 air780E 是否连接到服务端
 */
int air780e_is_connected_server(void)
{
    return _air780e_is_connected_server;
}

/**
 * 退出透传模式，断开服务端，关闭PDP上下文
 * @return 执行结果
 */
AT_RESULT air780e_disconnect_server_shut(void)
{
    AT_RESULT res;

    if (_air780e_is_connected_server == AIR780E_NOT_CONNECTED_SERVER)
        return AT_OK;

    /* 不管是否成功，都将标记位设置为断开服务端，如果不这样设置，程序会更加复杂 */
    _air780e_is_connected_server = AIR780E_NOT_CONNECTED_SERVER;

    air780e_exit_passthrough_mode();

    res = air780e_disconnect_server();
    if (res != AT_OK)
    {
        return res;
    }

    res = air780e_shut_pdp();
    if (res != AT_OK)
    {
        return res;
    }

    return AT_OK;
}
/**
 * 配置网络、激活PDP上下文、进入透传模式并连接服务器
 * @param ip        服务器 IP
 * @param port      服务器端口
 * @return 执行结果
 */
AT_RESULT air780e_activate_connect_server(const char *ip, int port)
{
    AT_RESULT res;
    static int n = 0;

    if (_air780e_is_connected_server == AIR780E_CONNECTED_SERVER)
        return AT_OK;

    n++;

    if (n >= 10)
    { // 连续 10 次调用这个函数，都没有连上服务端，就重启模组
        n = 0;
        Log_e("Failed to connect to server 10 times, restarting module...\n");
        air780e_reset();
        return AT_TIME_OUT;
    }

    res = air780e_ready();
    if (res != AT_OK)
    {
        Log_e("air780e_ready failed: %d\n", res);
        return res;
    }
    res = air780e_set_echo(AIR780E_ECHO_OFF);
    if (res != AT_OK)
    {
        Log_e("air780e_set_echo failed: %d\n", res);
        return res;
    }
    res = air780e_sim_unlock();
    if (res != AT_OK)
    {
        Log_e("air780e_sim_unlock failed: %d\n", res);
        return res;
    }
    res = air780e_register();
    if (res != AT_OK)
    {
        Log_e("air780e_register failed: %d\n", res);
        return res;
    }
    res = air780e_attached();
    if (res != AT_OK)
    {
        Log_e("air780e_attached failed: %d\n", res);
        return res;
    }
    res = air780e_set_passthrough_mode();
    if (res != AT_OK)
    {
        Log_e("air780e_set_passthrough_mode failed: %d\n", res);
        return res;
    }
    res = air780e_connect_server("TCP", ip, port);
    if (res != AT_OK)
    {
        Log_e("air780e_connect_server failed: %d\n", res);
        return res;
    }

    _air780e_is_connected_server = AIR780E_CONNECTED_SERVER;
    n = 0;

    return AT_OK;
}
// 任务函数
void air780e_at_task(void *pvParameters)
{
    uint32_t notify_bits = 0;
    char *data_to_send; // 待发送的数据接收缓冲区指针类型

    while (1)
    {
        // 透传模式时，永久阻塞在这一句
        xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&notify_bits, portMAX_DELAY);

        if (notify_bits & NOTIFY_START_AIR780E) // 启动模块，连接服务器
        {
            Log_d("NOTIFY_START_AIR780E\n");
            while (air780e_activate_connect_server(air780e_ip, air780e_port) != AT_OK)
            {
                os_delay_ms(100);
            }
            USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); // 使能空闲中断准备接受服务器数据，4G 模块透传模式接收数据的关键机制
            xTaskNotify(bike_task_handle, NOTIFY_CONNECTED_SERVER, eSetBits);
        }

        if (notify_bits & NOTIFY_SEND_DATA) // 发送数据到服务器
        {
            Log_d("NOTIFY_SEND_DATA\n");

            while (!is_empty(&cmd_queue))
            {
                if (dequeue(&cmd_queue, &data_to_send) == OK) // 从链式队列cmd_queue中出队数据字符串到data_to_send
                {
                    // 打印发送的数据
                    Log_d("send [%p] [%s] to server\n", data_to_send, data_to_send);
                    // 发送数据到服务器
                    at_passthrough_send(data_to_send, strlen(data_to_send));
                    os_free(data_to_send); // 释放字符串内存
                    data_to_send = NULL;
                }
            }
        }

        if (notify_bits & NOTIFY_USART_RX) // 从服务器收到数据
        {
            Log_d("NOTIFY_USART_RX\n");
            if (_air780e_is_connected_server == AIR780E_CONNECTED_SERVER)
            {
                // 透传模式下，直接从环形缓冲区里读取数据到recv_buf，不进行解析
                recv_data_len = at_passthrough_receive(recv_buf, sizeof(recv_buf));
                recv_buf[recv_data_len] = '\0';
                // 对收到的数据进行解析
                // 函数指针实际调用air780e_recv_data_cb函数
                recv_cb(recv_buf, recv_data_len); // 调用回调函数处理收到的数据:将收到的数据传递给外部应用层处理
            }
        }

        if (notify_bits & NOTIFY_STOP_AIR780E) // 断开连接
        {
            Log_d("NOTIFY_STOP_AIR780E\n");
            USART_ITConfig(USART2, USART_IT_IDLE, DISABLE); // 关闭空闲中断
            air780e_disconnect_server_shut();
        }
    }
}

void air780e_stop(void)
{
    xTaskNotify(at_task_handle, NOTIFY_STOP_AIR780E, eSetBits);
}

void air780e_start(const char *ip, int port, recv_cb_t cb)
{
    memset(air780e_ip, 0, sizeof(air780e_ip));
    strcpy(air780e_ip, ip);
    air780e_port = port;
    recv_cb = cb;

    xTaskNotify(at_task_handle, NOTIFY_START_AIR780E, eSetBits);
}
/**
 * 初始化AIR780E的控制脚
 */
void air780e_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(AIR780E_EN_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = AIR780E_EN_PIN;
    GPIO_Init(AIR780E_EN_PORT, &GPIO_InitStructure);

    at_init();
    air780e_reset();

    init_queue(&cmd_queue);

    os_mem_info();
    // 1K字节的栈空间
    xTaskCreate(air780e_at_task, "air780e_at_task", 256, NULL, 4,
                (TaskHandle_t *)&at_task_handle);
    os_mem_info(); // 显示内存使用情况

    /* AIR780E AT指令串口，注意波特率要和AIR780E模块配置相同 */
    usart2_init();
}
