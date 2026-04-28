#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "usart.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include "log.h"

static SemaphoreHandle_t log_mutex;

static char *level_str[] = {
    "DBG",
    "INF",
    "WRN",
    "ERR",
};
// __FILE__ 中包含了文件夹的名称，需要去掉文件夹名
static const char *_get_filename(const char *p)
{
    char ch = '\\';
    const char *q = strrchr(p, ch);
    if (q == NULL)
    {
        q = p;
    }
    else
    {
        q++;
    }
    return q;
}
/**
 * 全局日志级别输出标志, 只输出小于或等于该等级的日志信息
 */
static LOG_LEVEL log_level = LOG_DEBUG;
static char log_buf[LOG_MSG_MAX_LEN];
/* 线程安全输出日志 */
void os_log(const char *file, const int line, const int level, const char *fmt, ...)
{
    int offset = 0;

    if (level < log_level)
    {
        return;
    }

    const char *file_name = _get_filename(file);

    // 保护串口输出，防止任务切换导致数据错乱
    xSemaphoreTake(log_mutex, portMAX_DELAY);

    // 先把日志前缀写入buf，比如"INFO|main.c(123): "
    offset = sprintf(log_buf, "%s|%s(%d): ", level_str[level], file_name, line);

    va_list args;
    va_start(args, fmt);
    // 处理格式化字符串，将参数写入buf中，可输出文件名和行号等信息
    vsnprintf(log_buf + offset, sizeof(log_buf) - offset, fmt, args); // 从offset位置继续写，剩下的空间也要算
    va_end(args);

    usart1_send_string(log_buf);

    xSemaphoreGive(log_mutex);
}

/* 线程安全输出 */
void os_printf(const char *fmt, ...)
{
    // 保护串口输出，防止任务切换导致数据错乱
    xSemaphoreTake(log_mutex, portMAX_DELAY);

    va_list args;
    va_start(args, fmt);
    // 直接输出格式化字符串到串口，不使用buf
    vprintf(fmt, args); //  vprintf 处理 va_list，输出到串口
    va_end(args);

    xSemaphoreGive(log_mutex);
}

void log_init(LOG_LEVEL level)
{
    usart1_init();

    log_level = level;

    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex)
    {
        printf("log init wrong!\n");
    }
}
