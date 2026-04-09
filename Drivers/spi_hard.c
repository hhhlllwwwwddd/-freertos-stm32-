#include "spi.h"

/* 使用硬件实现 SPI */
#ifdef __USE_HARD_SPI_INTERFACE__
/**
  * @brief  初始化SPI外设
  * @param  无
  * @retval 无
  */
void spi_init(void)
{
    RCC_APB2PeriphClockCmd(SPI_CLK, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;             /* 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = SPI_CLK_PIN | SPI_MOSI_PIN;   /* 时钟和输出引脚 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_CLK_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;               /* 上拉输入 */
    GPIO_InitStructure.GPIO_Pin = SPI_MISO_PIN;                 /* 数据输入引脚 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_MISO_PORT, &GPIO_InitStructure);
    
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;               /* 设置SPI为主模式 */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; /* 设置为双线全双工模式 */
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;      /* 当SPI空闲时，SCK（时钟线）处于低电平 */
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;    /* 数据在SCLK的奇数边沿（此处是上升沿）被捕获或发送 */
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; /* 对时钟分频，72M/4=18M */
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;   /* 每次通信的数据大小为8位 */
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;  /* 数据传输从高有效位开始 */
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; /* 使用软件控制NSS线，而不是硬件控制 */
    SPI_InitStructure.SPI_CRCPolynomial = 0; /* CRC多项式校验，大多数从机不需要校验，这个变量无效 */
    SPI_Init(SPIx, &SPI_InitStructure);
    
    SPI_Cmd(SPIx, ENABLE);
}

/**
  * @brief  向MOSI写入一字节，并从MISO读取一字节
  * @param  byte_send 写入的字节
  * @retval 读取的字节
  */
uint8_t spi_read_write_byte(uint8_t byte_send)
{
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) != SET);
    SPI_I2S_SendData(SPIx, byte_send);
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) != SET);
    return SPI_I2S_ReceiveData(SPIx);
}

#endif
