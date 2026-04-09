#include "stdio.h"

#include "OLED.h"
#include "bike.h"
#include "utils.h"

void screen_clear(void)
{
    OLED_Clear();
}

void screen_welcome_show(char* time, int gps_enable)
{
    OLED_ShowString(1, 1, time);
    OLED_ShowString(2, 2, "Welcome to use");
    OLED_ShowString(3, 3, "Guo Guo Bike");
    
    if (!gps_enable) {
        OLED_ShowString(4, 2, "GPS NOT Ready!");
    } else {
        OLED_Clear_Part(4, 1, 16);
    }
}

void screen_1_show(char* time, float speed_kmh, float distance_km, float total_seconds, int bike_state)
{
    char oled_str[32] = {0};
    char* p;
    
    OLED_ShowString(1, 1, time);

    sprintf(oled_str, "%.02f km/h", speed_kmh);
    OLED_ShowString(2, 1, oled_str);
   
    sprintf(oled_str, "%.02f km", distance_km);
    OLED_ShowString(3, 1, oled_str);

    format_seconds(total_seconds, oled_str);
    OLED_ShowString(4, 1, oled_str);
    
    p = (bike_state == RUNNING) ? "go!" : "stop";
    OLED_ShowString(4, 11, p);
}

void screen_2_show(float max_speed_kmh, float average_speed_kmh, float kcal, float temp, float humi, int bike_state)
{
/*
 * 第二屏
 *  - 最大速度，单位km/h，范围 0.0km/h~99.99km/h
 *  - 平均速度，单位km/h，范围 0.0km/h~99.99km/h
 *  - 卡路里，单位KCAL
 *  - 实时温度，单位摄氏度；实时湿度，百分比
 */
    char oled_str[32] = {0};
    char* p;

    sprintf(oled_str, "%.02f km/h", max_speed_kmh);
    OLED_ShowString(1, 1, oled_str);

    sprintf(oled_str, "%.02f km/h", average_speed_kmh);
    OLED_ShowString(2, 1, oled_str);

    sprintf(oled_str, "%.02f KCAL", kcal);
    OLED_ShowString(3, 1, oled_str);

    sprintf(oled_str, "%.02fC %.02f%%", temp, humi);
    OLED_ShowString(4, 1, oled_str);
    
    p = (bike_state == RUNNING) ? "go!" : "stop";
    OLED_ShowString(1, 13, p);
}

void screen_init(void)
{
    /* 初始化OLED */
    OLED_Init();
}
