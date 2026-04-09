#ifndef DRIVE_HAL_H__
#define DRIVE_HAL_H__

#include "adc.h"
#include "foc.h"
#include <stdbool.h>

#define DRV_FREQ 16500

#define MCPWM_FOC_CURRENT_SAMP_OFFSET 2

#define SYSTEM_CORE_CLOCK (168000000)
#define HALF_CORE_CLOCK (SYSTEM_CORE_CLOCK / 2)

#define TIMER_UPDATE_SAMP(samp) TIM2->CCR2 = ((samp) / 2)

#define DC_CAL_ON() GPIOA->BSRRL = (1 << 15)
#define DC_CAL_OFF() GPIOA->BSRRH = (1 << 15)

#define EN_GATE_ON() GPIOC->BSRRL = (1 << 9)
#define EN_GATE_OFF() GPIOC->BSRRH = (1 << 9)

#define IS_FAULT() !(GPIOC->IDR & (1 << 13))

typedef struct
{
	uint8_t pwm_on;
	uint8_t mode;
	uint8_t mode_req;
	uint16_t err;
	bool init_done;

	float pwm_freq;
	float pwm_dt; // 1/pwm_freq
	float angle_sim;
	float angle_enc;

	float to;

	foc_ctrl_t foc;

	float in_iq;
	float in_id;
	float in_vel;
	float in_vel_rate;
	float in_pos;
} ctrl_t;

void drv_init(void);
void drv_init_constants(void);

void drv_change_freq(uint32_t freq);
void drv_upd_duty(float d0, float d1, float d2);

void drv_pwm_on(void);
void drv_pwm_off(void);

void drv_err_rst(void);
void drv_set_off(void);
void drv_set_ol(float cur, float pos, float vel, float vel_rate);
void drv_set_cur(float iq, float id);
void drv_set_vel(float vel, float vel_rate);

void drive_reset_master_to(void);

void DMA2_Stream4_IRQHandler(void);
void TIM2_IRQHandler(void);

extern volatile ctrl_t ctrl;

#endif // DRIVE_HAL_H__