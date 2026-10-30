#include "drive.h"
#include "graph.h"
#include "platform.h"
#include "proto.h"
#include "utils_math.h"

#define CUR_LIM 8 // max 8.25 @20.0mOhm
// #define CUR_LIM 4

#define TIMER_UPDATE_SAMP_TOP(samp, top) \
	TIM1->CR1 |= TIM_CR1_UDIS;           \
	TIM2->CR1 |= TIM_CR1_UDIS;           \
	TIM1->ARR = top;                     \
	TIM2->CCR2 = samp / 2;               \
	TIM1->CR1 &= ~TIM_CR1_UDIS;          \
	TIM2->CR1 &= ~TIM_CR1_UDIS;

#define TIMER_UPDATE_DUTY(duty1, duty2, duty3) \
	TIM1->CR1 |= TIM_CR1_UDIS;                 \
	TIM1->CCR3 = duty1;                        \
	TIM1->CCR2 = duty2;                        \
	TIM1->CCR1 = duty3;                        \
	TIM1->CR1 &= ~TIM_CR1_UDIS;

#define BW_SCALE 6
#define BLDC_R 0.48
#define BLDC_L 18e-6
#define TIMEOUT 2.0

volatile ctrl_t ctrl = {0};
int g_dir = 0;

void drive_reset_master_to(void) { ctrl.to = 0; }

void drv_init_constants(void)
{
	// float bandwidthFreq = ctrl.pwm_freq / (float)BW_SCALE;
	// ctrl.foc.i_reg.kp = BLDC_L * bandwidthFreq;
	// ctrl.foc.i_reg.ki = BLDC_R * bandwidthFreq;
	ctrl.foc.i_reg.kp = 0.5f;
	ctrl.foc.i_reg.ki = 50.0f;
	ctrl.vel_reg.kp = 0.1f;
	ctrl.vel_reg.ki = 3.0f;
	cplx_pi_reg_reset(&ctrl.foc.i_reg);
}

void drv_pwm_on(void)
{
	TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_OCMode_PWM1);
	TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Enable);

	TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_OCMode_PWM1);
	TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Enable);

	TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_OCMode_PWM1);
	TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Enable);
}

void drv_pwm_off(void)
{
	TIM_SelectOCxM(TIM1, TIM_Channel_1, TIM_ForcedAction_InActive);
	TIM_CCxCmd(TIM1, TIM_Channel_1, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);

	TIM_SelectOCxM(TIM1, TIM_Channel_2, TIM_ForcedAction_InActive);
	TIM_CCxCmd(TIM1, TIM_Channel_2, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);

	TIM_SelectOCxM(TIM1, TIM_Channel_3, TIM_ForcedAction_InActive);
	TIM_CCxCmd(TIM1, TIM_Channel_3, TIM_CCx_Enable);
	TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);

	TIM_GenerateEvent(TIM1, TIM_EventSource_COM);
}

void drv_change_freq(uint32_t freq)
{
	if(freq < 2000) freq = 2000;
	if(freq > 20000) freq = 20000;
	uint32_t top = HALF_CORE_CLOCK / freq - 1;
	TIMER_UPDATE_SAMP_TOP(MCPWM_FOC_CURRENT_SAMP_OFFSET, top);
	ctrl.pwm_dt = 1 / freq;
	ctrl.pwm_freq = freq;
	TIMER_UPDATE_DUTY(TIM1->ARR / 2, TIM1->ARR / 2, TIM1->ARR / 2);
}

void DMA2_Stream4_IRQHandler(void)
{
	if(DMA_GetFlagStatus(DMA2_Stream4, DMA_FLAG_HTIF4) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream4, DMA_FLAG_HTIF4);
	}
	if(DMA_GetFlagStatus(DMA2_Stream4, DMA_FLAG_TCIF4) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream4, DMA_FLAG_TCIF4);
	}
}

#define MCCONF_FOC_PLL_KP 2000.0
#define MCCONF_FOC_PLL_KI 30000.0

static inline void foc_pll_run(float phase, float dt, volatile float *phase_var, volatile float *speed_var)
{
	UTILS_NAN_ZERO(*phase_var);
	float delta_theta = phase - *phase_var;
	delta_theta = utils_norm_angle_rad(delta_theta);
	UTILS_NAN_ZERO(*speed_var);
	*phase_var += (*speed_var + MCCONF_FOC_PLL_KP * delta_theta) * dt;
	*phase_var = utils_norm_angle_rad(*phase_var);
	*speed_var += MCCONF_FOC_PLL_KI * delta_theta * dt;
}

void TIM2_IRQHandler(void)
{
	// GPIOB->ODR |= 1 << 12;
	if(TIM_GetITStatus(TIM2, TIM_IT_CC2) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
		bool is_v7 = !(TIM1->CR1 & TIM_CR1_DIR);

		ctrl.to += ctrl.pwm_dt;

		if(is_v7)
		{
			// GPIOB->ODR &= ~(1 << 12);
			return;
		}

		ctrl.tick++;

		adc_conv_val();
		gr_poll();
		if(ctrl.init_done)
		{
			TIM_GenerateEvent(TIM1, TIM_EventSource_COM); // Generate COM event here for synchronization

			// track errors
			if(IS_FAULT()) ctrl.err |= 1 << ERR_FAULT;
			if(g_adc_val.tcpu >= 80 || g_adc_val.tinv > 80) ctrl.err |= 1 << ERR_OT;
			if(g_adc_val.ub > 18) ctrl.err |= 1 << ERR_OV;
			if(g_adc_val.ub < 11) ctrl.err |= 1 << ERR_UV;
			if(ctrl.to > TIMEOUT) ctrl.err |= 1 << ERR_TO;
			if(fabsf(g_adc_val.i[0]) > CUR_LIM || fabsf(g_adc_val.i[1]) > CUR_LIM) ctrl.err |= 1 << ERR_OC;

			ctrl.hall_smpl_cnt++;
			for(uint32_t i = 0; i < 2; i++)
			{
				ctrl.hall[i].sum += g_adc_val.hall_raw[i];
				if(ctrl.hall[i].min > g_adc_val.hall_raw[i]) ctrl.hall[i].min = g_adc_val.hall_raw[i];
				if(ctrl.hall[i].max < g_adc_val.hall_raw[i]) ctrl.hall[i].max = g_adc_val.hall_raw[i];
			}

			if(ctrl.hall_cal[0].span > 0 && ctrl.hall_cal[0].avg > 500 &&
			   ctrl.hall_cal[1].span > 0 && ctrl.hall_cal[1].avg > 500)
			{
				ctrl.angle_enc = utils_fast_atan2(((float)g_adc_val.hall_raw[0] - ctrl.hall_cal[0].avg) / ctrl.hall_cal[0].span,
												  ((float)g_adc_val.hall_raw[1] - ctrl.hall_cal[1].avg) / ctrl.hall_cal[1].span);
				ctrl.angle_foc = utils_norm_angle_rad(ctrl.angle_enc * ctrl.pp + ctrl.enc_offset);
			}

			if(ctrl.err)
			{
				// EN_GATE_OFF();
				if(ctrl.pwm_on)
				{
					ctrl.pwm_on = false;
					drv_pwm_off();
				}
				if(ctrl.mode_req == DRV_MODE_ERROR) // try to clear errors
				{
					ctrl.err = 0;
					EN_GATE_ON();
				}
			}
			else
			{
				EN_GATE_ON();
				ctrl.mode = ctrl.mode_req;

				if(ctrl.mode == DRV_MODE_OFF)
				{
					foc_mon(&ctrl.foc);
					if(ctrl.pwm_on)
					{
						ctrl.pwm_on = false;
						drv_pwm_off();
					}
				}
				else if(ctrl.mode == DRV_MODE_OL ||
						ctrl.mode == DRV_MODE_CURRENT ||
						ctrl.mode == DRV_MODE_VELOCITY)
				{
					float a = ctrl.angle_foc;
					if(ctrl.mode == DRV_MODE_OL)
					{
						ctrl.angle_sim += ctrl.in_vel * ctrl.pwm_dt;
						ctrl.angle_sim = fmodf(ctrl.angle_sim, 2.0f * M_PI);

						a = ctrl.in_vel == 0 ? ctrl.in_pos : ctrl.angle_sim;
					}

					if(ctrl.mode == DRV_MODE_VELOCITY)
					{
						ctrl.vel_ramp += CLIPS(ctrl.in_vel - ctrl.vel_ramp, ctrl.in_vel_rate * ctrl.pwm_dt);
						// if(fabsf(ctrl.m_speed_est_fast) < RPM2RADPS_f(MCCONF_FOC_HALL_INTERP_ERPM)) kp *= 0.1f;
						pi_reg_upd(&ctrl.vel_reg, ctrl.vel_ramp - ctrl.m_speed_est_fast, CUR_LIM * .9f, ctrl.pwm_dt);
						ctrl.in_id = 0;
						ctrl.in_iq = ctrl.vel_reg.out;
					}
					foc_calc(&ctrl.foc, ctrl.pwm_dt,
							 g_adc_val.ub * 0.93, g_adc_val.i[g_dir ? 1 : 0], g_adc_val.i[g_dir ? 0 : 1],
							 ctrl.in_iq, ctrl.in_id, 0, ctrl.m_pll_speed * 0, a);

					if(ctrl.pwm_on == false)
					{
						ctrl.pwm_on = true;
						drv_pwm_on();
					}

					drv_upd_duty(ctrl.foc.pwm[g_dir ? 1 : 0], ctrl.foc.pwm[g_dir ? 0 : 1], ctrl.foc.pwm[2]);
				}
			}
			if(ctrl.mode != DRV_MODE_VELOCITY)
			{
				ctrl.vel_ramp = ctrl.m_speed_est_fast;
				pi_reg_reset(&ctrl.vel_reg);
			}

			foc_pll_run(ctrl.angle_enc, ctrl.pwm_dt, &ctrl.m_pll_phase, &ctrl.m_pll_speed);
			{
				float diff = utils_angle_difference_rad(ctrl.angle_enc, ctrl.m_phase_before_speed_est);
				utils_truncate_number(&diff, -M_PI / 3.0, M_PI / 3.0);

				UTILS_LP_FAST(ctrl.m_speed_est_fast, diff / ctrl.pwm_dt, 0.01);
				UTILS_NAN_ZERO(ctrl.m_speed_est_fast);

				UTILS_LP_FAST(ctrl.m_speed_est_faster, diff / ctrl.pwm_dt, 0.2);
				UTILS_NAN_ZERO(ctrl.m_speed_est_faster);

				float diff_corr = utils_angle_difference_rad(ctrl.angle_enc, ctrl.m_phase_before_speed_est_corrected);
				utils_truncate_number(&diff_corr, -M_PI / 3.0, M_PI / 3.0);

				UTILS_LP_FAST(ctrl.m_speed_est_fast_corrected, diff_corr / ctrl.pwm_dt, 0.01);
				UTILS_NAN_ZERO(ctrl.m_speed_est_fast_corrected);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
				utils_truncate_number_abs(&ctrl.m_pll_speed, fabsf(ctrl.m_speed_est_fast) * 3.0); // pll wind-up protection
#pragma GCC diagnostic pop

				ctrl.i_abs = NORM2_f(ctrl.foc.id_fb_filt, ctrl.foc.iq_fb_filt);
				UTILS_LP_FAST(ctrl.i_abs_filt, ctrl.i_abs, 0.001);

				ctrl.m_phase_before_speed_est = ctrl.angle_enc;
				ctrl.m_phase_before_speed_est_corrected = ctrl.angle_enc;

				// ctrl.rpm = RADPS2RPM_f(ctrl.m_pll_speed); // Note that this value has to be divided by half the number of motor poles.
				ctrl.rpm = (ctrl.m_pll_speed); // Note that this value has to be divided by half the number of motor poles.
				// ctrl.rpm_fast = RADPS2RPM_f(ctrl.m_speed_est_fast);
				ctrl.rpm_fast = (ctrl.m_speed_est_fast);
			}
		}
		gr_set_state(ctrl.mode, ctrl.err);
	}
	// GPIOB->ODR &= ~(1 << 12);
}

void drv_upd_duty(float d0, float d1, float d2)
{
	d0 += 0.5f;
	d1 += 0.5f;
	d2 += 0.5f;
	d0 = CLIP(d0, 0, 1.0f);
	d1 = CLIP(d1, 0, 1.0f);
	d2 = CLIP(d2, 0, 1.0f);
	const float scale = TIM1->ARR;
	TIMER_UPDATE_DUTY((uint32_t)(d0 * scale),
					  (uint32_t)(d1 * scale),
					  (uint32_t)(d2 * scale));
}

void drv_err_rst(void)
{
	ctrl.mode_req = DRV_MODE_ERROR;
}

void drv_set_off(void)
{
	ctrl.mode_req = DRV_MODE_OFF;
	ctrl.in_iq = 0;
	ctrl.in_id = 0;
	ctrl.in_vel = 0;
	ctrl.in_vel_rate = 0;
}

void drv_set_ol(float iq, float id, float pos, float vel, float vel_rate)
{
	ctrl.in_iq = iq;
	ctrl.in_id = id;
	ctrl.in_vel = vel;
	ctrl.in_vel_rate = vel_rate;
	ctrl.in_pos = utils_norm_angle_rad(pos);
	ctrl.mode_req = DRV_MODE_OL;
}

void drv_set_cur(float iq, float id)
{
	ctrl.mode_req = DRV_MODE_CURRENT;
	ctrl.in_iq = iq;
	ctrl.in_id = id;
}

void drv_set_vel(float vel, float vel_rate)
{
	ctrl.mode_req = DRV_MODE_VELOCITY;
	ctrl.in_vel = vel;
	ctrl.in_vel_rate = vel_rate;
}