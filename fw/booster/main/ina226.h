#ifndef INA226_H__
#define INA226_H__

#include "console.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_INA_NUM I2C_NUM_1
#define I2C_MASTER_FREQ_HZ 400000
#define INA226_ADDR 0x40

typedef struct
{
	uint16_t volt_raw, volt_raw_filt;
	int16_t cur_raw;
	float cur, volt;

	uint32_t err;
	uint32_t cnt_acq;
	bool ready, al_en;
} ina226_t;

void ina226_init(void);
void ina226_poll(uint32_t diff_ms);

void ina226_trace(print_func_t pf);

uint32_t ina226_get_alert_count(void);
bool ina226_is_alert(void);
void ina226_reset_alert(void);

void ina226_cur_limit_en(bool en);

extern ina226_t ina226;

#endif // INA226_H__