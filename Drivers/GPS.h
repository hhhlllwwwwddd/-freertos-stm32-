#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x_conf.h"

int is_revevied_GPRMC(void);
void parse_GPRMC(void);
void gps_data_revevied(uint8_t rx_data);
int gps_enable(void);
char* gps_get_time_str(void);
float gps_get_speed_kt(void);
char* gps_get_latitude(void);
char* gps_get_longitude(void);
#endif
