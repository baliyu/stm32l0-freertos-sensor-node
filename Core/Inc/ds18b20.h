#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>

#define DS18B20_NOT_FOUND INT16_MIN

/* Start the 1 MHz microsecond timer used for 1-Wire timing */
void ds18b20_init(void);

/* Temperature in hundredths of a degree C (2312 = 23.12 C), or DS18B20_NOT_FOUND */
int16_t ds18b20_read_centi(void);

#endif
