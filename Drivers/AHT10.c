#include "AHT10.h"

/* AHT10地址 */
#define AHT10_ADDRESS (0x38 << 1) /* 0x70 */

/**
 * @brief  复位AHT10
 * @param  无
 * @retval 无
 */
void aht10_reset(void)
{
    /* 1011 1010 */
    uint8_t reset = 0xBA;
    /* 发送指令软复位 */
    i2c_write(AHT10_ADDRESS, &reset, 1);

    /* 根据手册，复位不超过 20 毫秒 */
    os_delay_ms(20);
}
/**
 * @brief  读取AHT10数据
 * @param  *humi 湿度，百分比
 * @param  *temp 温度，单位：摄氏度
 * @retval 0 - 读取成功；-1 - 读取失败
 */
int aht10_read(float *humi, float *temp)
{
    uint8_t humi_temp[6]; /* 声明变量存放读取的数据 */
    /*  10101100, 00110011, 00000000 */
    uint8_t measure[3] = {0xAC, 0x33, 0x00};

    /* 发送指令触发测量 */
    i2c_write(AHT10_ADDRESS, measure, 3);
    /* 根据手册，需要延时至少 75 毫秒等待测量完成，测量完成设备会变为空闲状态 */
    os_delay_ms(80);
    /* 读取6个字节 */
    i2c_read(AHT10_ADDRESS, humi_temp, 6);

    /* 0x08(00001000)检查状态字节第 3 位(校准使能位)，如为0，则传感器未校准，不准确 */
    if ((humi_temp[0] & 0x08) == 0)
    {
        aht10_reset();
        return 0;
    }
    else if ((humi_temp[0] & 0x80) == 0)
    {                                                                                     /* 0x80(10000000)检查状态字节第7位(忙闲指示)，0 说明测量完成 */
        uint32_t SRH = (humi_temp[1] << 12) | (humi_temp[2] << 4) | (humi_temp[3] >> 4);  /* 湿度数据处理 */
        uint32_t ST = ((humi_temp[3] & 0x0f) << 16) | (humi_temp[4] << 8) | humi_temp[5]; /* 温度数据处理 */

        *humi = (SRH * 100.0) / 1024.0 / 1024;     /* 根据手册公式转换湿度数据 */
        *temp = (ST * 200.0) / 1024.0 / 1024 - 50; /* 根据手册公式转换温度数据 */

        return 0;
    }
    return -1;
}
