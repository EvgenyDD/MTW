#ifndef DRIVE_HAL_H__
#define DRIVE_HAL_H__

#include "adc.h"
#include "foc.h"
#include <stdbool.h>

#define DRV_FREQ 16000

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
	uint32_t min, max;
	uint32_t sum;
} hall_acc_t;

typedef struct
{
	float avg;
	float span;
} hall_calib_t;

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

	float to;
	uint32_t tick;

	foc_ctrl_t foc;
	pi_reg_t vel_reg;
	float vel_ramp;

	float in_iq;
	float in_id;
	float in_vel;
	float in_vel_rate;
	float in_pos;

	float m_ang_hall;
	int m_ang_hall_int_prev;
	float m_hall_dt_diff_last;
	float m_hall_dt_diff_now;
	float m_phase_before_speed_est;
	float m_phase_before_speed_est_corrected;

	float m_speed_est_fast;
	float m_speed_est_faster;
	float m_speed_est_fast_corrected;

	int ang_hall_int;

	float m_pll_phase;
	float m_pll_speed; // elec. rad/s

	float i_abs, i_abs_filt;

	float rpm, rpm_fast;

	hall_acc_t hall[2];
	hall_calib_t hall_cal[2];
	float angle_enc;
	float angle_foc;
	uint32_t hall_smpl_cnt;

	float pp;
	float enc_offset;
} ctrl_t;

void drv_init(void);
void drv_init_constants(void);
void drv_upd_deadtime_ns(float dt);

void drv_change_freq(uint32_t freq);
void drv_upd_duty(float d0, float d1, float d2);

void drv_pwm_on(void);
void drv_pwm_off(void);

void drv_err_rst(void);
void drv_set_off(void);
void drv_set_ol(float iq, float id, float pos, float vel, float vel_rate);
void drv_set_cur(float iq, float id);
void drv_set_vel(float vel, float vel_rate);

void drive_reset_master_to(void);

void DMA2_Stream4_IRQHandler(void);
void TIM2_IRQHandler(void);

extern volatile ctrl_t ctrl;
extern int g_dir;

#endif // DRIVE_HAL_H__