#include "drive.h"
#include "platform.h"

static int calculate_deadtime(float deadtime_ns, float timer_clock_freq_hz) // 5142ns max
{
	float t_dts = 1000000000.0f / timer_clock_freq_hz; // ns

	uint8_t DTG = 0;
	if(deadtime_ns <= (127.0f * t_dts))
	{
		DTG = (uint8_t)(deadtime_ns / t_dts);
	}
	else if(deadtime_ns <= (63.0f * 2.0f * t_dts + 64.0f * t_dts))
	{
		DTG = (uint8_t)((deadtime_ns / (2.0f * t_dts)) - 64.0f);
		DTG |= 0x80; // Set bit 7
	}
	else if(deadtime_ns <= (31.0f * 8.0f * t_dts + 32.0f * 2.0f * t_dts + 64.0f * t_dts))
	{
		DTG = (uint8_t)((deadtime_ns / (8.0f * t_dts)) - 32.0f);
		DTG |= 0xC0; // Set bits 7 and 6
	}
	else if(deadtime_ns <= (31.0f * 16.0f * t_dts + 32.0f * 8.0f * t_dts + 32.0f * 2.0f * t_dts + 64.0f * t_dts))
	{
		DTG = (uint8_t)((deadtime_ns / (16.0f * t_dts)) - 32.0f);
		DTG |= 0xE0; // Set bits 7, 6, and 5
	}
	else
	{
		return 255;
	}

	return DTG;
}

static void drv_spi_init(void)
{
}

void drv_init(void)
{
	uint32_t frequency = DRV_FREQ;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	drv_spi_init();

	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; // PWM - H
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15; // PWM - L
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_TIM1);

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15; // DC_CAL
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; // EN_GATE
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	EN_GATE_OFF();
	DC_CAL_OFF();

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13; // FAULT
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // ADC
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4; // ADC
	GPIO_Init(GPIOC, &GPIO_InitStructure);

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
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	ctrl.pwm_freq = frequency;
	ctrl.pwm_dt = 1.0f / ctrl.pwm_freq;

	drv_init_constants();

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
	TIM_BDTRInitStruct.TIM_DeadTime = calculate_deadtime(500.0f, SYSTEM_CORE_CLOCK);
	TIM_BDTRInitStruct.TIM_Break = TIM_Break_Disable;
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

	drv_pwm_off();

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15; // DC_CAL
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIMER_UPDATE_SAMP(MCPWM_FOC_CURRENT_SAMP_OFFSET);

	TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE); // Enable CC2 interrupt, which will be fired in V0 and V7

	__NVIC_SetPriority(TIM2_IRQn, 6);
	__NVIC_ClearPendingIRQ(TIM2_IRQn);
	__NVIC_EnableIRQ(TIM2_IRQn);

	ctrl.init_done = true;

	drv_upd_duty(0, 0, 0);

	EN_GATE_ON();
}

#warning "heal deadtime"