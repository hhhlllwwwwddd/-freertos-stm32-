#ifndef __UTILS_H
#define __UTILS_H

#include "stm32f10x_conf.h"

/* 无符号整数转成缩小100倍的浮点数 */
float uint16_to_float(uint16_t value);
/* 浮点数转成放大100倍的无符号整数 */
uint16_t float_to_uint16(float value);
/* 有符号整数转成缩小100倍的浮点数 */
float int16_to_float(int16_t value);
/* 浮点数转成放大100倍的有符号整数 */
int16_t float_to_int16(float value);

/*
 * 将秒数，转换成 hh:mm:ss 的格式
 * 比如 100 秒，应当转成 00:01:40
 * 3700秒，应当转成 01:01:40
 */
char *format_seconds(int total_seconds, char *formatted_time);
/* 整数时间戳转成字符串*/
char *timestamp_to_time_str(uint32_t t, char *time_str);
/* 时间字符串转换成整数时间戳 */
uint32_t time_str_to_timestamp(char *time_str);

/* WGS-84坐标转换为GCJ-02坐标 */
void gps_to_gcj02(double wgs_lat, double wgs_lon, double *gcj_lat, double *gcj_lon);
/* 将度、分形式转换为十进制的度形式 */
double convert_to_decimal_degrees(double degree_minutes);

/* 计算平均温湿度 */
void calc_average_humi_temp(float humi, float temp,
                            float *average_humi, float *average_temp, int reset_data);
/* 计算 sec 秒的骑行距离 */
float calc_distance_m(float speed_kt, uint32_t sec);
/* 计算骑行总距离 */
float calc_total_distance_km(float distance_km, int reset_data);
/* 计算消耗卡路里 */
float calc_kcal(float total_distance_km);
/* 计算实时速度 */
float calc_speed_kmh(float speed_kt);
/* 计算最高时速 */
float calc_max_speed_kmh(float speed_kmh, int reset_data);
/* 计算平均时速 */
float calc_average_speed(float total_distance_km, uint32_t total_time_sec);

#endif