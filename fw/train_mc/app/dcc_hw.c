#include "dcc.h"
#include "platform.h"

static volatile uint8_t txBuffer[256] = "abcd";
static volatile uint8_t text[16] = {"012"};

void dcc_hw_init(void)
{
	{ // uart - debug
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
		GPIO_InitTypeDef GPIO_InitStruct;
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
		GPIO_InitStruct.GPIO_Speed = GPIO_Fast_Speed;
		GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
		GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);

		USART_InitTypeDef USART_InitStruct;
		USART_InitStruct.USART_BaudRate = 1000000;
		USART_InitStruct.USART_WordLength = USART_WordLength_8b;
		USART_InitStruct.USART_StopBits = USART_StopBits_1;
		USART_InitStruct.USART_Parity = USART_Parity_No;
		USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		USART_InitStruct.USART_Mode = USART_Mode_Tx;
		USART_Init(USART2, &USART_InitStruct);

		USART_Cmd(USART2, ENABLE);
		USART_SendData(USART2, 0);
	}

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource2);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource3);

	EXTI_InitTypeDef EXTI_InitStruct;
	EXTI_InitStruct.EXTI_Line = EXTI_Line2;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	EXTI_InitStruct.EXTI_Line = EXTI_Line3;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = EXTI2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);

	NVIC_InitStruct.NVIC_IRQChannel = EXTI3_IRQn;
	NVIC_Init(&NVIC_InitStruct);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
	TIM_TimeBaseStruct.TIM_Prescaler = 84 - 1;
	TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStruct.TIM_Period = 29 - 1; // 29us by std. ?
	TIM_TimeBaseStruct.TIM_ClockDivision = 0;
	TIM_TimeBaseStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);

	TIM_TimeBaseStruct.TIM_Period = 0xFFFF;
	TIM_TimeBaseInit(TIM14, &TIM_TimeBaseStruct);

	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

	NVIC_InitStruct.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);

	TIM_Cmd(TIM3, DISABLE);
	TIM_Cmd(TIM14, DISABLE);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10; // uart tx
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 250000;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_Mode = USART_Mode_Tx;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_Init(USART3, &USART_InitStructure);

	USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);
	USART_Cmd(USART3, ENABLE);

	DMA_DeInit(DMA1_Stream3);

	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_Channel = DMA_Channel_4;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DR;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)txBuffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStructure.DMA_BufferSize = 1;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA1_Stream3, &DMA_InitStructure);

	// DMA_ITConfig(DMA1_Stream3, DMA_IT_TC, ENABLE);

	// NVIC_InitStruct.NVIC_IRQChannel = DMA1_Stream3_IRQn;
	// NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
	// NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	// NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	// NVIC_Init(&NVIC_InitStruct);

	TIM3->CR1 |= TIM_CR1_URS;

	TIM_Cmd(TIM3, ENABLE);
	TIM_Cmd(TIM14, ENABLE);
}

static void trigger_send(uint8_t *data, uint16_t size)
{
	DMA_Cmd(DMA1_Stream3, DISABLE);
	while(DMA_GetCmdStatus(DMA1_Stream3) == ENABLE)
		;
	DMA_SetCurrDataCounter(DMA1_Stream3, size);
	DMA_MemoryTargetConfig(DMA1_Stream3, (uint32_t)data, DMA_Memory_0);
	DMA_ClearFlag(DMA1_Stream3, DMA_FLAG_TCIF3 | DMA_FLAG_FEIF3 | DMA_FLAG_DMEIF3 | DMA_FLAG_TEIF3 | DMA_FLAG_HTIF3);
	DMA_Cmd(DMA1_Stream3, ENABLE);
}

static void rec_dcc_bit(uint32_t cnt, bool pol)
{
#ifdef DBG_STM
	uint8_t v = cnt > 255 ? 255 : cnt;
	u2_tx(&v, 1);
#endif

	// clang-format off
	int8_t bit_type = DCC_BIT_UNDEF;
	if     (cnt > DCC_LEN_0_MIN  && cnt < DCC_LEN_0_MAX)  bit_type = DCC_BIT_0;
	else if(cnt > DCC_LEN_1_MIN  && cnt < DCC_LEN_1_MAX)  bit_type = DCC_BIT_1;
	else if(cnt > DCC_LEN_00_MIN && cnt < DCC_LEN_00_MAX) bit_type = DCC_BIT_00;
	else if(cnt > DCC_LEN_01_MIN && cnt < DCC_LEN_01_MAX) bit_type = DCC_BIT_01;
	else if(cnt > DCC_LEN_10_MIN && cnt < DCC_LEN_10_MAX) bit_type = DCC_BIT_10;
	else if(cnt > DCC_LEN_11_MIN && cnt < DCC_LEN_11_MAX) bit_type = DCC_BIT_11;
	// clang-format on

	dcc_dc_push(pol ? bit_type : -bit_type);
}

void EXTI2_IRQHandler(void)
{
	volatile const uint32_t c = TIM14->CNT;
	if(EXTI_GetITStatus(EXTI_Line2) != RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line2);
		if(c > DCC_LEN_00_MIN)
		{
			TIM3->EGR = TIM_EventSource_Update;
			TIM14->EGR = TIM_EventSource_Update;
			TIM3->CR1 |= TIM_CR1_CEN;
			// volatile uint32_t dummy = TIM3->SR;
			// (void)dummy;
			// TIM3->SR = (uint16_t)~TIM_FLAG_Update;
			// TIM14->SR = (uint16_t)~TIM_FLAG_Update;
			rec_dcc_bit(c, 0);
		}
		__DSB();
	}
}

void EXTI3_IRQHandler(void)
{
	volatile const uint32_t c = TIM14->CNT;
	if(EXTI_GetITStatus(EXTI_Line3) != RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line3);
		if(c > DCC_LEN_00_MIN)
		{
			TIM3->EGR = TIM_EventSource_Update;
			TIM14->EGR = TIM_EventSource_Update;
			TIM3->CR1 |= TIM_CR1_CEN;
			// volatile uint32_t dummy = TIM3->SR;
			// (void)dummy;
			// TIM3->SR = (uint16_t)~TIM_FLAG_Update;
			// TIM14->SR = (uint16_t)~TIM_FLAG_Update;
			rec_dcc_bit(c, 1);
		}
		__DSB();
	}
}

void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
		TIM_Cmd(TIM3, DISABLE);
		if((GPIOC->IDR & (1 << 2)) == 0 &&
		   (GPIOC->IDR & (1 << 3)) == 0)
		{
			// railcom
			//  if(++text[0] > '9') text[0] = '0';
			text[0]++;
			trigger_send(text, 3);
			// GPIOB->ODR ^= 1 << 3;
		}
	}
}

void DMA1_Stream3_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_Stream3, DMA_IT_TCIF3))
	{
		DMA_ClearITPendingBit(DMA1_Stream3, DMA_IT_TCIF3);
	}
}

void u2_tx(const uint8_t *data, uint16_t length)
{
	for(uint16_t i = 0; i < length; i++)
	{
		while((USART2->SR & USART_SR_TXE) == 0)
			;
		USART_SendData(USART2, data[i]);
	}
	// while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)	;
}