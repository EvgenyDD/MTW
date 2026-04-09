#include "drive.h"
#include "graph.h"
#include "platform.h"
#include "proto.h"
#include "utils_math.h"

#define TIMER_UPDATE_SAMP_TOP(samp, top) \
	TIM1->CR1 |= TIM_CR1_UDIS;           \
	TIM2->CR1 |= TIM_CR1_UDIS;           \
	TIM1->ARR = top;                     \
	TIM2->CCR2 = samp / 2;               \
	TIM1->CR1 &= ~TIM_CR1_UDIS;          \
	TIM2->CR1 &= ~TIM_CR1_UDIS;

#define TIMER_UPDATE_DUTY(duty1, duty2, duty3) \
	TIM1->CR1 |= TIM_CR1_UDIS;                 \
	TIM1->CCR1 = duty1;                        \
	TIM1->CCR2 = duty3;                        \
	TIM1->CCR3 = duty2;                        \
	TIM1->CR1 &= ~TIM_CR1_UDIS;

#define BW_SCALE 6
#define BLDC_R 0.48
#define BLDC_L 18e-6
#define TIMEOUT 2.0

volatile ctrl_t ctrl = {0};

void drive_reset_master_to(void) { ctrl.to = 0; }

void drv_init_constants(void)
{
	float bandwidthFreq = ctrl.pwm_freq / (float)BW_SCALE;
	ctrl.foc.i_reg.kp = BLDC_L * bandwidthFreq;
	ctrl.foc.i_reg.ki = BLDC_R * bandwidthFreq;
	CplxPiRegReset(&ctrl.foc.i_reg);
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
	uint32_t top = HALF_CORE_CLOCK / freq - 1;
	TIMER_UPDATE_SAMP_TOP(MCPWM_FOC_CURRENT_SAMP_OFFSET, top);
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

const volatile uint32_t *pwm_switchTable[][3] = {
	{&TIM1->CCR1, &TIM1->CCR2, &TIM1->CCR3},
	{&TIM1->CCR1, &TIM1->CCR3, &TIM1->CCR2},
	{&TIM1->CCR2, &TIM1->CCR1, &TIM1->CCR3},
	{&TIM1->CCR2, &TIM1->CCR3, &TIM1->CCR1},
	{&TIM1->CCR3, &TIM1->CCR1, &TIM1->CCR2},
	{&TIM1->CCR3, &TIM1->CCR2, &TIM1->CCR1}};

void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2, TIM_IT_CC2) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
		bool is_v7 = !(TIM1->CR1 & TIM_CR1_DIR);

		ctrl.to += ctrl.pwm_dt;

		if(is_v7) return;

		adc_conv_val();
		gr_poll();
		if(ctrl.init_done)
		{
			TIM_GenerateEvent(TIM1, TIM_EventSource_COM); // Generate COM event here for synchronization

			// track errors
			if(IS_FAULT()) ctrl.err |= 1 << ERR_FAULT;
			if(g_adc_val.tcpu >= 80 || g_adc_val.tinv > 80) ctrl.err |= 1 << ERR_OT;
			if(g_adc_val.ub > 18) ctrl.err |= 1 << ERR_OV;
			if(g_adc_val.ub < 12) ctrl.err |= 1 << ERR_UV;
			if(ctrl.to > TIMEOUT) ctrl.err |= 1 << ERR_TO;
			if(fabsf(g_adc_val.i[0]) > 5 || fabsf(g_adc_val.i[1]) > 5) ctrl.err |= 1 << ERR_OC;

			if(ctrl.err)
			{
				// EN_GATE_OFF();
				if(ctrl.pwm_on)
				{
					drv_pwm_off();
					ctrl.pwm_on = false;
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
					float a = ctrl.angle_enc;
					if(ctrl.mode == DRV_MODE_OL)
					{
						ctrl.angle_sim += ctrl.in_vel * ctrl.pwm_dt;
						ctrl.angle_sim = fmodf(ctrl.angle_sim, 2.0f * M_PI);
						a = ctrl.angle_sim;
					}
					foc_calc(&ctrl.foc, ctrl.pwm_dt,
							 g_adc_val.ub, g_adc_val.i[0], g_adc_val.i[1],
							 ctrl.in_iq, ctrl.in_id, 0, 0, a);

					if(ctrl.pwm_on == false)
					{
						ctrl.pwm_on = true;
						drv_pwm_on();
					}
					drv_upd_duty(ctrl.foc.pwm[0], ctrl.foc.pwm[1], ctrl.foc.pwm[2]);
				}
				else
				{
					foc_mon(&ctrl.foc);
				}
			}
		}
		gr_set_state(ctrl.mode, ctrl.err);
	}
}

void drv_upd_duty(float d0, float d1, float d2)
{
	d0 = CLIP(d0, -1.0f, 1.0f);
	d1 = CLIP(d1, -1.0f, 1.0f);
	d2 = CLIP(d2, -1.0f, 1.0f);
	TIMER_UPDATE_DUTY((uint32_t)((d0 + 1.0f) * 0.5 * (float)TIM1->ARR),
					  (uint32_t)((d1 + 1.0f) * 0.5 * (float)TIM1->ARR),
					  (uint32_t)((d2 + 1.0f) * 0.5 * (float)TIM1->ARR));
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

void drv_set_ol(float cur, float pos, float vel, float vel_rate)
{
	ctrl.mode_req = DRV_MODE_OL;
	ctrl.in_iq = cur;
	ctrl.in_id = 0;
	ctrl.in_vel = vel;
	ctrl.in_vel_rate = vel_rate;
	ctrl.in_pos = pos;
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