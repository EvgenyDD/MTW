#include "drive_hal.h"
#include "graph.h"
#include "platform.h"
#include "utils_math.h"
#include <string.h>

#define SYSTEM_CORE_CLOCK (168000000)
#define HALF_CORE_CLOCK (SYSTEM_CORE_CLOCK / 2)

#define MCPWM_FOC_CURRENT_SAMP_OFFSET 2

#define DC_CAL_ON() GPIOA->BSRRL = (1 << 15)
#define DC_CAL_OFF() GPIOA->BSRRH = (1 << 15)

#define EN_GATE_ON() GPIOC->BSRRL = (1 << 9)
#define EN_GATE_OFF() GPIOC->BSRRH = (1 << 9)

#define TIMER_UPDATE_SAMP(samp) TIM2->CCR2 = (samp / 2);

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

volatile uint16_t g_adc_buffer[ADC_CH_COUNT] = {0};
volatile adc_val_t g_adc_val = {0};

volatile ctrl_t ctrl = {0};

static uint8_t conf_general_calculate_deadtime(float deadtime_ns, float core_clock_freq)
{
	float timebase = 1.0 / (core_clock_freq / 1000000.0f) * 1000.0f;

	uint8_t DTG = 0;
	if(deadtime_ns <= (timebase * 127.0))
	{
		DTG = deadtime_ns / timebase;
	}
	else
	{
		if(deadtime_ns <= ((63.0 + 64.0) * 2.0 * timebase))
		{
			DTG = deadtime_ns / (2.0 * timebase) - 64.0;
			DTG |= 0x80;
		}
		else
		{
			if(deadtime_ns <= ((31.0 + 32.0) * 8.0 * timebase))
			{
				DTG = deadtime_ns / (8.0 * timebase) - 32.0;
				DTG |= 0xC0;
			}
			else
			{
				if(deadtime_ns <= ((31.0 + 32) * 16 * timebase))
				{
					DTG = deadtime_ns / (16.0 * timebase) - 32.0;
					DTG |= 0xE0;
				}
				else
				{
					DTG = 0xFF;
					while(1)
						;
				}
			}
		}
	}
	return DTG;
}

static void pwm_enable(void)
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

static void pwm_disable(void)
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

	// TIM_GenerateEvent(TIM1, TIM_EventSource_COM);
	TIM1->EGR = TIM_EventSource_COM;
}

void drive_init(void)
{
	// uint32_t frequency = 14500;
	uint32_t frequency = DRV_FREQ;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; // PWM - H
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;						 // temporary out to disable PWM outputs during init
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIOA->BSRRH = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15; // PWM - L
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;						   // temporary out to disable PWM outputs during init
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIOB->BSRRH = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_TIM1);

	// ADC
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// TIM1 (main PWM)
	TIM_DeInit(TIM1);
	TIM_DeInit(TIM2);
	TIM1->CNT = 0;
	TIM2->CNT = 0;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2 | RCC_APB2Periph_ADC3, ENABLE);

	// DMA for the ADC
	DMA_InitTypeDef DMA_InitStructure = {0};
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&g_adc_buffer;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC->CDR;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = ADC_CH_COUNT;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream4, &DMA_InitStructure);

	DMA_Cmd(DMA2_Stream4, ENABLE);
	DMA_ITConfig(DMA2_Stream4, DMA_IT_HT, ENABLE);
	// DMA_ITConfig(DMA2_Stream4, DMA_IT_TC, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream4_IRQn;
	NVIC_Init(&NVIC_InitStructure);

	adc_init();

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_Period = HALF_CORE_CLOCK / ((uint32_t)frequency) - 1;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 1; // Only generate update event on underflow
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	ctrl.pwm_freq = frequency;
	ctrl.pwm_dt = 1.0f / frequency;
	float dt = (float)((TIM1->PSC + 1) * (TIM1->ARR + 1)) / (float)HALF_CORE_CLOCK;

	float bandwidthFreq = frequency / COLLECTOR_CONFIG_VALUE(Config_loopBandwidth);
	ctrl.foc->iRegParam.kp = COLLECTOR_CONFIG_VALUE(Config_phInd) * bandwidthFreq * 1e-6;
	ctrl.foc->iRegParam.ki = COLLECTOR_CONFIG_VALUE(Config_phRes) * bandwidthFreq / frequency;
	CplxPiRegReset(&ctrl.foc->i_reg);

	TIM_OCInitTypeDef TIM_OCInitStructure = {0};
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_Pulse = TIM1->ARR / 2;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;

	TIM_OC1Init(TIM1, &TIM_OCInitStructure);
	TIM_OC2Init(TIM1, &TIM_OCInitStructure);
	TIM_OC3Init(TIM1, &TIM_OCInitStructure);

	TIM_BDTRInitTypeDef TIM_BDTRInitStruct = {0};
	TIM_BDTRInitStruct.TIM_OSSRState = TIM_OSSRState_Enable;
	TIM_BDTRInitStruct.TIM_OSSIState = TIM_OSSIState_Enable;
	TIM_BDTRInitStruct.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
	TIM_BDTRInitStruct.TIM_DeadTime = conf_general_calculate_deadtime(500.0f, SYSTEM_CORE_CLOCK);
	TIM_BDTRInitStruct.TIM_Break = TIM_Break_Enable;
	TIM_BDTRInitStruct.TIM_BreakPolarity = TIM_BreakPolarity_Low;
	TIM_BDTRInitStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
	TIM_BDTRConfig(TIM1, &TIM_BDTRInitStruct);

	TIM_ARRPreloadConfig(TIM1, ENABLE);
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
	TIM_CCPreloadControl(TIM1, ENABLE);

	// TIM2 (trigger ADC)

	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 250;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);
	TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_OC3Init(TIM2, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);

	TIM_ARRPreloadConfig(TIM2, ENABLE);
	TIM_CCPreloadControl(TIM2, ENABLE);

	TIM_CtrlPWMOutputs(TIM2, ENABLE); // PWM outputs have to be enabled in order to trigger ADC on CCx

	TIM_SelectOutputTrigger(TIM1, TIM_TRGOSource_Update);
	TIM_SelectMasterSlaveMode(TIM1, TIM_MasterSlaveMode_Enable);
	TIM_SelectInputTrigger(TIM2, TIM_TS_ITR0);
	TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);

	TIM1->CNT = 0;
	TIM_Cmd(TIM1, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);

	pwm_disable();

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; // PWM - H
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15; // BREAK + PWM - L
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15; // DC_CAL
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIMER_UPDATE_SAMP(MCPWM_FOC_CURRENT_SAMP_OFFSET);

	TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE); // Enable CC2 interrupt, which will be fired in V0 and V7

	__NVIC_SetPriority(TIM2_IRQn, 6);
	__NVIC_ClearPendingIRQ(TIM2_IRQn);
	__NVIC_EnableIRQ(TIM2_IRQn);

	ctrl.init_done = true;
}

void drv_change_freq(uint32_t freq)
{
	uint32_t top = HALF_CORE_CLOCK / freq - 1;
	TIMER_UPDATE_SAMP_TOP(MCPWM_FOC_CURRENT_SAMP_OFFSET, top);
	TIMER_UPDATE_DUTY(TIM1->ARR / 2, TIM1->ARR / 2, TIM1->ARR / 2);
}

void DMA2_Stream4_IRQHandler(void)
{
	GPIOB->ODR ^= GPIO_Pin_3;
	if(DMA_GetFlagStatus(DMA2_Stream4, DMA_FLAG_HTIF4) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream4, DMA_FLAG_HTIF4);
		GPIOB->ODR ^= GPIO_Pin_6;
	}
	if(DMA_GetFlagStatus(DMA2_Stream4, DMA_FLAG_TCIF4) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream4, DMA_FLAG_TCIF4);
		GPIOB->ODR ^= GPIO_Pin_7;
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
		adc_conv_val();
		gr_poll();
		if(ctrl.init_done)
		{
			TIM_GenerateEvent(TIM1, TIM_EventSource_COM); // Generate COM event here for synchronization

			/// main loop
			// foc(&ctrl.foc, ctrl.mode == CTRL_MODE_CUR, ctrl.pwm_dt, ctrl.pwm_freq, g_adc_val[], iq, is, );
			if(is_foc)
			{
				// servoPwm.setPwm(&state.pwm);
				{
					uint32_t top = *sett.topPtr;

					state.duty.x[0] = (uint32_t)((duty->x[0] + 1.0) * (float)*sett.topPtr) >> 1;
					state.duty.x[1] = (uint32_t)((duty->x[1] + 1.0) * (float)*sett.topPtr) >> 1;
					state.duty.x[2] = (uint32_t)((duty->x[2] + 1.0) * (float)*sett.topPtr) >> 1;

					if(state.duty.x[0] > top) state.duty.x[0] = top;
					if(state.duty.x[1] > top) state.duty.x[1] = top;
					if(state.duty.x[2] > top) state.duty.x[2] = top;

					HAL_updateDuty(&state.duty, COLLECTOR_CONFIG_VALUE(Config_focOffset) >= 0.0 ? sett.cwPwmSelector : sett.ccwPwmSelector);
					{

						TIM1->CR1 |= TIM_CR1_UDIS;

						*(uint32_t *)pwm_switchTable[selector][0] = vect->x[0];
						*(uint32_t *)pwm_switchTable[selector][1] = vect->x[1];
						*(uint32_t *)pwm_switchTable[selector][2] = vect->x[2];

						TIM1->CR1 &= ~TIM_CR1_UDIS;
					}
				}
			}
			// servoPwm.enablePwm();
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

				TIM_GenerateEvent(TIM1, TIM_EventSource_COM);
			}
		}
		gr_set_state(ctrl.mode, ctrl.err);
	}
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
}
TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
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