#ifndef __FLASH_DATA_H
#define __FLASH_DATA_H

#include "stm32f10x_conf.h"
#include "bike.h"

/**
  * @brief  打印第n个表的内容，即输出第n次骑行数据
  * @param  n 表编号，1~16，对应索引下标0~15
  * @retval 无
  */
void print_table_n(int n);
/**
  * @brief  保存一个数据项到记录中
  * @param  lat 纬度
  * @param  lon 经度
  * @param  lspeed_kmh 速度
  * @param  bike_state 运行状态，如果是停止状态，则需要立即写入flash
  * @retval 无
  */
void save_item(char* lat, char* lon, float speed_kmh, enum BIKE_STATE bike_state);
/**
  * @brief  设置第一次写入数据标记位
  * @param  无
  * @retval 无
  */
void set_first_write_flag(void);
/**
  * @brief  初始化flash
  * @param  无
  * @retval 无
  */
void init_flash(void);

#endif
