#ifndef DRIVE_HAL_H__
#define DRIVE_HAL_H__

#include "adc.h"
#include "foc.h"
#include <stdbool.h>

#define DRV_FREQ 16500

typedef struct
{
	uint8_t pwm_on;
	uint32_t mode;
	uint32_t err;
	bool init_done;

	float pwm_freq;
	float pwm_dt;

	foc_ctrl_t foc;
} ctrl_t;

void drive_init(void);

void DMA2_Stream4_IRQHandler(void);
void TIM2_IRQHandler(void);

void drv_change_freq(uint32_t freq);
void drv_upd_duty(float d0, float d1, float d2);

extern volatile ctrl_t ctrl;

#endif // DRIVE_HAL_H__