#include "string.h"
#include "stdio.h"
#include "bike.h"
#include "GPS.h"

static struct
{
    char time_str[18];  /**< 格式：24-06-27 14:47:55 */
    float speed_kt;     /**< 单位节，海里每小时 */
    char latitude[12];  /**< 实时纬度的原始数据，单位度分 */
    char longitude[13]; /**< 实时经度的原始数据，单位度分 */
    int enable;         /**< GPS是否有效定位了 */
} gps = {
    .time_str = "24-01-01 00:00:00",
    .speed_kt = 0.0,
    .latitude = "0",
    .longitude = "0",
    .enable = 0};

int gps_enable(void)
{
    return gps.enable;
}
char *gps_get_time_str(void)
{
    return gps.time_str;
}
float gps_get_speed_kt(void)
{
    return gps.speed_kt;
}
char *gps_get_latitude(void)
{
    return gps.latitude;
}
char *gps_get_longitude(void)
{
    return gps.longitude;
}
// 将UTC时间和日期转换为北京时间
static void UTC_to_BJT(const char *utc_time, const char *utc_date, char *bjt_time, char *bjt_date)
{
    int hour, minute, second, day, month, year;
    int days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // 解析UTC时间和日期字符串
    sscanf(utc_time, "%2d%2d%2d", &hour, &minute, &second);
    sscanf(utc_date, "%2d%2d%2d", &day, &month, &year);

    // 闰年的2月是29天
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        days_in_month[1] = 29;

    // 增加8小时
    hour += 8;

    // 处理超过24小时的情况
    if (hour >= 24)
    {
        hour -= 24;
        day += 1;
        // 处理月份的最后一天
        if (day > days_in_month[month - 1])
        {
            day = 1;
            month += 1;
            // 处理年份的最后一天
            if (month > 12)
            {
                month = 1;
                year += 1;
            }
        }
    }

    // 格式化为北京时间字符串
    snprintf(bjt_date, 9, "%02d-%02d-%02d", year, month, day);
    snprintf(bjt_time, 9, "%02d:%02d:%02d", hour, minute, second);
}

#define GPRMC_BUFFER_SIZE 128
static char gprmc_buffer[GPRMC_BUFFER_SIZE];
static uint8_t gprmc_received = 0;

// 解析 GPRMC 句子
void parse_GPRMC(void)
{
    int field_index = 0, field_len = 0;
    const char *start = gprmc_buffer;
    const char *end;

    char utc_time[11] = {0}, utc_date[7] = {0}, speed_str[12] = {0}, bjt_date[9] = {0}, bjt_time[9] = {0};
    gprmc_received = 0; // 复位标志位

    /* $GPRMC, 060322.00, A, 4003.520920,N, 11620.220964,E,  0.354, ,    010624,      ,   ,  A*72
     * 头,     时间,    状态, 纬度,  方向,   经度,  方向, 速度, 航向, 日期, 磁偏角,指示器,校验和
     */
    if (strncmp(gprmc_buffer, "$GPRMC", 6) == 0)
    {
        while ((end = strstr(start, ",")) != NULL)
        {
            field_len = (int)(end - start);
            /* 查看解析出的每个字段 */
            // printf("field [%d]: %.*s\n", field_index, field_len, start);

            switch (field_index)
            {
            case 1: // 时间
                memcpy(utc_time, start, field_len);
                utc_time[sizeof(utc_time) - 1] = '\0';
                break;
            case 2: // 状态
                if (memcmp("A", start, field_len) == 0)
                    gps.enable = 1;
                else if (memcmp("V", start, field_len) == 0)
                    gps.enable = 0;
                break;
            case 3: // 纬度
                strncpy(gps.latitude, start, field_len);
                gps.latitude[sizeof(gps.latitude) - 1] = '\0';
                break;
            case 4: // 纬度方向（N/S）
                if (memcmp("S", start, field_len) == 0)
                {
                    gps.latitude[0] = '-';
                }
                break;
            case 5: // 经度
                strncpy(gps.longitude, start, field_len);
                gps.longitude[sizeof(gps.longitude) - 1] = '\0';
                break;
            case 6: // 经度方向（E/W）
                if (memcmp("W", start, field_len) == 0)
                {
                    gps.longitude[0] = '-';
                }
                break;
            case 7: // 速度，单位：节
                strncpy(speed_str, start, field_len);
                speed_str[sizeof(speed_str) - 1] = '\0';
                break;
            case 9: // 日期
                strncpy(utc_date, start, field_len);
                utc_date[sizeof(utc_date) - 1] = '\0';
                break;
            default:
                break;
            }
            start = end + 1;
            field_index++;
        }

        // printf("UTC Date: %s\r\n", utc_date);      // UTC 日期
        // printf("UTC Time: %s\r\n", utc_time);      // UTC 时间
        //  将UTC时间和日期转换为北京时间和日期
        if (strlen(utc_time) && strlen(utc_date))
        {
            UTC_to_BJT(utc_time, utc_date, bjt_time, bjt_date);
            sprintf(gps.time_str, "%s %s", bjt_date, bjt_time);
        }

        sscanf(speed_str, "%f", &gps.speed_kt);

        // printf("s %s\r\n", speed_str);     // 纬度
        // printf("Beijing Time: 20%s %s\r\n", bjt_date, bjt_time);
        // printf("Longitude: %s\r\n", longitude);   // 经度
        // printf("Latitude: %s\r\n", latitude);     // 纬度
        // printf("-----------------------------------------\r\n");
    }
}

#define RX_BUFFER_SIZE 1024
static char rx_buffer[RX_BUFFER_SIZE];
static uint16_t rx_index = 0;
/* 是否收到一条完整的GPRMC句子 */
int is_revevied_GPRMC(void)
{
    return gprmc_received;
}
/**
 * @brief  处理串口收到的GPS数据
 * @param  串口每次收到的字节
 * @retval 无
 */
void gps_data_revevied(uint8_t rx_data)
{
    BaseType_t taskWoken = pdFALSE;

    rx_buffer[rx_index++] = rx_data;

    // 检测到换行符，表示一条NMEA句子结束
    if (rx_data == '\n')
    {
        rx_buffer[rx_index] = '\0'; // 确保字符串以NULL结尾

        // 检查是否是GPRMC句子
        if (strncmp(rx_buffer, "$GPRMC", 6) == 0)
        {
            strncpy((char *)gprmc_buffer, rx_buffer, RX_BUFFER_SIZE);
            gprmc_received = 1; // 收到了完整的一条GPRMC数据，并已将其拷贝至 gprmc_buffer 中

            xTaskNotifyFromISR(
                bike_task_handle,
                NOTIFY_GPRMC_RECV,
                eSetBits, // 设置标志位
                &taskWoken);
            portYIELD_FROM_ISR(taskWoken);
        }

        rx_index = 0; // 复位索引，准备接收下一条句子
    }
}
