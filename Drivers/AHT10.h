#ifndef __AHT10_H
#define __AHT10_H

#include "i2c.h"

/**
  * @brief  读取AHT10数据
  * @param  *humi 湿度，百分比
  * @param  *temp 温度，单位：摄氏度
  * @retval 0 - 读取成功；-1 - 读取失败
  */
int aht10_read(float *humi, float *temp);

#endif
