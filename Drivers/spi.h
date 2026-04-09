#ifndef __SPI_H
#define __SPI_H

#include "stm32f10x_conf.h"
#include "system.h"

#define SPIx                                SPI1
#define SPI_CLK                             (RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1)
#define SPI_GPIO_CLK                        RCC_APB2Periph_GPIOA
#define SPI_CLK_PORT			            GPIOA
#define SPI_CLK_PIN			                GPIO_Pin_5

#define SPI_MISO_PORT			            GPIOA
#define SPI_MISO_PIN			            GPIO_Pin_6

#define SPI_MOSI_PORT			            GPIOA
#define SPI_MOSI_PIN			            GPIO_Pin_7

/**
  * @brief  初始化 SPI 相关配置
  * @param  无
  * @retval 无
  */
void spi_init(void);
/**
  * @brief  向MOSI写入一字节，并从MISO读取一字节
  * @param  byte_send 写入的字节
  * @retval 读取的字节
  */
uint8_t spi_read_write_byte(uint8_t byte_send);

/* 选择使用硬件还是软件方式实现 SPI，只能打开一个宏定义 */
#define __USE_HARD_SPI_INTERFACE__
//#define __USE_SOFT_SPI_INTERFACE__

#endif
