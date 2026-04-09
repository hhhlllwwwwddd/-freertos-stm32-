#ifndef __BIKE_H
#define __BIKE_H

#include "stm32f10x_conf.h"
#include "system.h"

/*
* 0:初始进入RUNNING
* 1:已连接服务器
* 2:已成功更新时间
*/
enum RUNNING_STATE {
    RUNNING_NOT_CONNECTED_SERVER,
    RUNNING_CONNECTED_SERVER,
    RUNNING_UPDATED_TIME
};
/*
* 0:空闲，上电后初始，屏幕展示：Welcome to use Guo Guo Bike
* 1:运行，骑行开始，按秒统计上述数据
* 2:停止，本次骑行结束，屏幕显示最后一次统计的数据
*/
enum BIKE_STATE {
    IDEL,
    RUNNING,
    STOP,
};

float bike_get_distance_km(void);
char* bike_get_time_str(void);
float bike_get_max_speed_kmh(void);
float bike_get_average_speed_kmh(void);
float bike_get_kcal(void);
float bike_get_average_humi(void);
float bike_get_average_temp(void);

void bike_data_exprot_request(char* cmd);
void bike_data_list_request(void);

void bike_handle(void);
void bike_init(void);

void bike_start(void);

extern TaskHandle_t bike_task_handle;
#define NOTIFY_KEY1_PRESS          (1 << 0)
#define NOTIFY_KEY2_PRESS          (1 << 1)
#define NOTIFY_GPRMC_RECV          (1 << 2)
#define NOTIFY_CONNECTED_SERVER    (1 << 3)
#define NOTIFY_TIME_UPDATE         (1 << 4)
#define NOTIFY_DATA_LIST           (1 << 5)
#define NOTIFY_DATA_EXPORT         (1 << 6)

#endif
