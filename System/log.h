#ifndef __LOG_H
#define __LOG_H

#define LOG_MSG_MAX_LEN (512)

/**
 * 日志输出等级
 */
typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LOG_LEVEL;

/**
 * @brief 日志打印函数，默认打印到串口1
 *
 * @param file 源文件名
 * @param line 行号
 * @param level 日志等级
 */
void os_log(const char *file, const int line, const int level, const char *fmt, ...);
/**
 * @brief 日志功能初始化
 * @param level 日志等级，只输出小于或等于该等级的日志信息
 */
void log_init(LOG_LEVEL level);

#define Log_d(args...) os_log(__FILE__, __LINE__, LOG_DEBUG, args)
#define Log_i(args...) os_log(__FILE__, __LINE__, LOG_INFO, args)
#define Log_w(args...) os_log(__FILE__, __LINE__, LOG_WARN, args)
#define Log_e(args...) os_log(__FILE__, __LINE__, LOG_ERROR, args)
/* 线程输出 */
void os_printf(const char *fmt, ...);

#endif
