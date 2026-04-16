#include "W25Qxx.h"
#include "stdio.h"

#define W25Qxx_NSS_PORT GPIOA
#define W25Qxx_NSS_CLK RCC_APB2Periph_GPIOA
#define W25Qxx_NSS_PIN GPIO_Pin_4

#define W25Qxx_NSS_HIGH() GPIO_WriteBit(W25Qxx_NSS_PORT, W25Qxx_NSS_PIN, Bit_SET)
#define W25Qxx_NSS_LOW() GPIO_WriteBit(W25Qxx_NSS_PORT, W25Qxx_NSS_PIN, Bit_RESET)
/**
 * @brief  初始化w25qxx，主要是初始化片选信号线
 * @param  无
 * @retval 无
 */
void w25qxx_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(W25Qxx_NSS_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = W25Qxx_NSS_PIN;    /* 片选引脚 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(W25Qxx_NSS_PORT, &GPIO_InitStructure);

    W25Qxx_NSS_HIGH();
}
/**
 * @brief  读取设备的 JEDEC ID
 * @param  MID 制造商ID，Manufacturer ID，winbond固定为 0xEF
 * @param  DID 设备ID，Device ID，W25Q16 是 0x4015
 * @retval 无
 */
void w25qxx_read_id(uint8_t *MID, uint16_t *DID)
{
    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_JEDEC_ID);
    *MID = spi_read_write_byte(W25Qxx_DUMMY_BYTE);
    *DID = spi_read_write_byte(W25Qxx_DUMMY_BYTE);
    *DID <<= 8;
    *DID |= spi_read_write_byte(W25Qxx_DUMMY_BYTE);

    W25Qxx_NSS_HIGH();
}
/**
 * @brief  开启写使能，即关闭写保护
 * @param  无
 * @retval 无
 */
static void w25qxx_write_enable(void)
{
    W25Qxx_NSS_LOW();
    spi_read_write_byte(W25Qxx_WRITE_ENABLE);
    W25Qxx_NSS_HIGH();
}
/**
 * @brief  读取芯片状态，如果忙则等待，一直等到其空闲，或者100微秒超时
 * @param  无
 * @retval 无
 */
static void w25qxx_wait_for_idel(void)
{
    uint8_t retry = 0;

    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_READ_STATUS_REGISTER_1);

    while ((spi_read_write_byte(W25Qxx_DUMMY_BYTE) & W25Qxx_REGISTER_1_BUSY_BIT) && retry < 100)
    {
        retry++;
        os_delay(1);
    }

    W25Qxx_NSS_HIGH();
}
/**
 * @brief  按扇区擦除
 * @param  addr flash内部地址
 * @retval 无
 */
void w25qxx_sector_erase(uint32_t addr)
{
    w25qxx_write_enable();

    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_SECTOR_ERASE_4KB);
    spi_read_write_byte(addr >> 16);
    spi_read_write_byte(addr >> 8);
    spi_read_write_byte(addr);

    W25Qxx_NSS_HIGH();

    w25qxx_wait_for_idel();

    os_delay_ms(30); /* 擦除后给芯片一点反应时间 */
}
/**
 * @brief  擦除整片
 * @retval 无
 */
void w25qxx_chip_erase(void)
{
    w25qxx_write_enable();

    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_CHIP_ERASE);

    W25Qxx_NSS_HIGH();

    w25qxx_wait_for_idel();

    os_delay_ms(30); /* 擦除后给芯片一点反应时间 */
}
/**
 * @brief  按页写入，向地址addr写入data_size个字节数据
 * @param  addr flash内部地址
 * @param  data 数据首地址
 * @param  data_size 数据长度
 * @retval 无
 */
void w25qxx_page_write_bytes(uint32_t addr, uint8_t *data, uint32_t data_size)
{
    w25qxx_write_enable();

    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_PAGE_PROGRAM);
    spi_read_write_byte(addr >> 16);
    spi_read_write_byte(addr >> 8);
    spi_read_write_byte(addr);

    while (data_size--)
        spi_read_write_byte(*data++);

    W25Qxx_NSS_HIGH();

    w25qxx_wait_for_idel();

    os_delay_ms(5); /* 写入后给芯片一点反应时间 */
}
/**
 * @brief  从地址addr读取data_size个字节数据
 * @param  addr flash内部地址
 * @param  data 数据首地址
 * @param  data_size 数据长度
 * @retval 无
 */
void w25qxx_read_bytes(uint32_t addr, uint8_t *data, uint32_t data_size)
{
    W25Qxx_NSS_LOW();

    spi_read_write_byte(W25Qxx_READ_DATA);
    spi_read_write_byte(addr >> 16);
    spi_read_write_byte(addr >> 8);
    spi_read_write_byte(addr);

    while (data_size--)
        *data++ = spi_read_write_byte(W25Qxx_DUMMY_BYTE);

    W25Qxx_NSS_HIGH();
}

/* 打印十六进制数组 */
void print_hex_array(const char *prefix, uint8_t *array, int size)
{
    int i;
    printf("%s\r\n", prefix);
    for (i = 0; i < size; i++)
    {
        printf("0x%02X ", array[i]);
        if (i % 16 == 15)
        {
            printf("\r\n");
        }
    }
}
/**
 * @brief  驱动接口测试函数，用于读写测试
 * @param  无
 * @retval 无
 */
void w25qxx_test(void)
{
#define W25QXX_TEST_LEN 256
    uint8_t write_buf[W25QXX_TEST_LEN];
    uint8_t read_buf[W25QXX_TEST_LEN];
    int i;
    uint8_t MID;
    uint16_t DID;

    printf("SPI Flash W25QXX Test\r\n");
    spi_init();
    w25qxx_init();

    w25qxx_read_id(&MID, &DID);

    printf("MID:0x%X, DID:0x%X\r\n", MID, DID);
    /* 这里只校验了制造商ID，严格来讲可以同时校验设备ID */
    if (MID != 0xEF)
    {
        printf("SPI Flash W25QXX test fail\r\n");
        return;
    }
    /* 从0地址开始，擦除4K内容，擦除后0~4095地址的内容会变成0xFF */
    w25qxx_sector_erase(0x00);
    os_delay(10); /* 擦除后给芯片一点反应时间 */
    for (i = 0; i < W25QXX_TEST_LEN; i++)
    {
        write_buf[i] = i;
    }

    print_hex_array("============[write data]============", write_buf, W25QXX_TEST_LEN);
    w25qxx_page_write_bytes(0x00, write_buf, W25QXX_TEST_LEN);
    os_delay(10); /* 写入后给芯片一点反应时间 */

    w25qxx_read_bytes(0x00, read_buf, W25QXX_TEST_LEN);
    print_hex_array("============[read data]============", read_buf, W25QXX_TEST_LEN);

    for (i = 0; i < W25QXX_TEST_LEN; i++)
    {
        if (read_buf[i] != write_buf[i])
        {
            printf("0x%02X ", read_buf[i]);
            printf("SPI Flash W25QXX test fail\r\n");
            return;
        }
    }
    printf("SPI Flash W25QXX test success\r\n");
}
