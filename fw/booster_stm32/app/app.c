#include "adc.h"
#include "config_system.h"
#include "console.h"
#include "fw_header.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"
#include "usb_hw.h"
#include "usbd_core_cdc.h"
#include <string.h>

// #define SYSTICK_IN_US (168000000 / 1000000)
#define SYSTICK_IN_MS (168000000 / 1000)

bool g_stay_in_boot = false;
uint32_t g_uid[3];
volatile uint32_t system_time_ms = 0;

uint8_t rf_channel = 42;
config_entry_t g_device_config[] = {{"rf_chnl", sizeof(rf_channel), 0, &rf_channel}};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

#define UART3_RX_DMA_BUFFER_SIZE 256

static uint8_t uart3_rx_buffer[UART3_RX_DMA_BUFFER_SIZE];
static volatile uint8_t uart3_rx_active = 0;

static int32_t prev_systick = 0;

uint8_t age = 0;

void delay_ms(volatile uint32_t delay_ms)
{
	volatile uint32_t start = 0;
	int32_t mark_prev = 0;
	prof_mark(&mark_prev);
	const uint32_t time_limit = delay_ms * SYSTICK_IN_MS;
	for(;;)
	{
		start += (uint32_t)prof_mark(&mark_prev);
		if(start >= time_limit)
			return;
	}
}

static void consume(const uint8_t *data, size_t len);

static void UART3_StopReception(void)
{
	GPIOB->ODR &= ~(1 << 7);

	consume(uart3_rx_buffer, UART3_RX_DMA_BUFFER_SIZE - DMA1_Stream1->NDTR);

	if(uart3_rx_active == 0) return;
	DMA_Cmd(DMA1_Stream1, DISABLE);

	while(DMA_GetCmdStatus(DMA1_Stream1) != DISABLE)
		;

	USART_ClearFlag(USART3, USART_FLAG_FE | USART_FLAG_NE | USART_FLAG_ORE | USART_FLAG_PE);
	DMA_ClearFlag(DMA1_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);

	DMA1_Stream1->NDTR = UART3_RX_DMA_BUFFER_SIZE;

	uart3_rx_active = 0;
}

static void UART3_ResumeReception(void)
{
	GPIOB->ODR |= (1 << 7);

	if(uart3_rx_active) return;
	DMA1_Stream1->M0AR = (uint32_t)uart3_rx_buffer;
	DMA1_Stream1->NDTR = UART3_RX_DMA_BUFFER_SIZE;
	USART_ClearFlag(USART3, USART_FLAG_FE | USART_FLAG_NE | USART_FLAG_ORE | USART_FLAG_PE);
	DMA_ClearFlag(DMA1_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
	DMA_Cmd(DMA1_Stream1, ENABLE);
	uart3_rx_active = 1;
}

static volatile uint32_t ph = 0;

void TIM2_IRQHandler(void)
{
	if(TIM2->SR & TIM_SR_UIF)
	{
		TIM2->SR &= ~TIM_SR_UIF;

		if(++ph >= 5)
		{
			ph = 0;
			UART3_StopReception();
		}

		uint32_t v = GPIOB->ODR;
		v &= ~((1 << 10) | (1 << 11));
		if(ph == 4)
		{
			age++;
			v |= (1 << 10) | (1 << 11);

			GPIOB->ODR = v;
			for(volatile uint32_t i = 0; i < 100; i++)
				asm("nop");
			TIM2->CNT = 450 - 1;
			UART3_ResumeReception();
		}
		else
		{

			if(ph & 1)
				v |= 1 << 10;
			else
				v |= 1 << 11;
			GPIOB->ODR = v;

			if((ph % 4) < 2)
				TIM2->CNT = 58 - 1;
			else
				TIM2->CNT = 100 - 1;
		}
	}
}

static struct
{
	uint32_t tot;
	uint32_t last;
	uint32_t min;
	uint32_t max;

	uint32_t cnt_ok;
	uint32_t cnt_wr;
	uint32_t cnt_wm;

	uint32_t lastcorr[3];
	
} stat = {0};

uint8_t wm[128][8]={0};
uint8_t fb[512] = {0};
static uint32_t cfb = 0;

uint32_t ptr_wm = 0;

static void consume(const uint8_t *data, size_t len)
{
	if(stat.max < len) stat.max = len;
	if(stat.min > len) stat.min = len;

	if(len != 3)
	{
		stat.cnt_wr++;
		if(len <= 8 && len != 0)
		{
			memset(wm[ptr_wm], 0xAA, 8);
			memcpy(wm[ptr_wm], data, len);
			if(++ptr_wm >= 128) ptr_wm = 0;
		}
	}
	else
	{
		if(memcmp(data, "012", 3) == 0)
			stat.cnt_ok++;
		else
		{
			stat.cnt_wr++;
			stat.cnt_wm++;
			memcpy(stat.lastcorr, data, len);
		}
	}

	static uint8_t last_age = 255;
	if(last_age != age)
	{
		last_age = age;
		stat.last = 0;
	}
	else
	{
		stat.last++;
	}
	stat.tot += len;
	for(uint32_t i = 0; i < len; i++)
	{
		if(cfb >= 512) cfb = 0;
		fb[cfb++] = data[i];
	}
}

void main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	platform_init();
	adc_init();

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from app

	if(config_validate() == CONFIG_STS_OK) config_read_storage();

	usb_init();

	// A15 EN
	// C11 UART RX
	// B11 IN1
	// B10 IN2
	// B6 PULLUP uart

	{
		GPIO_InitTypeDef GPIO_InitStructure = {0};
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		GPIOA->ODR |= (1 << 15);

		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_10 | GPIO_Pin_11;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
		GPIOB->ODR |= (1 << 6);

		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; // uart3 rx
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
		GPIO_Init(GPIOC, &GPIO_InitStructure);
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);

		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

		USART_InitTypeDef USART_InitStructure;
		USART_InitStructure.USART_BaudRate = 250000;
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;
		USART_InitStructure.USART_StopBits = USART_StopBits_1;
		USART_InitStructure.USART_Parity = USART_Parity_No;
		USART_InitStructure.USART_Mode = USART_Mode_Rx;
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		USART_Init(USART3, &USART_InitStructure);

		DMA_DeInit(DMA1_Stream1);

		DMA_InitTypeDef DMA_InitStructure;
		DMA_InitStructure.DMA_Channel = DMA_Channel_4;
		DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DR;
		DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)uart3_rx_buffer;
		DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
		DMA_InitStructure.DMA_BufferSize = UART3_RX_DMA_BUFFER_SIZE;
		DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
		DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
		DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
		DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
		DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
		DMA_InitStructure.DMA_Priority = DMA_Priority_High;
		DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
		DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
		DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
		DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
		DMA_Init(DMA1_Stream1, &DMA_InitStructure);

		DMA_Cmd(DMA1_Stream1, ENABLE);
		USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE);
		USART_Cmd(USART3, ENABLE);
		while(!(USART3->SR & USART_SR_TXE))
			;
		uart3_rx_active = 1;

		UART3_StopReception();

		RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
		TIM2->PSC = 168 / 2 - 1;
		TIM2->ARR = 65535;
		TIM2->CNT = 999;
		TIM2->CR1 |= TIM_CR1_DIR; // downcounting
		TIM2->DIER |= TIM_DIER_UIE;
		NVIC_EnableIRQ(TIM2_IRQn);
		NVIC_SetPriority(TIM2_IRQn, 1);
		TIM2->CR1 |= TIM_CR1_CEN;
	}

	stat.min = 5000;
	stat.max = 0;
	for(;;)
	{
		uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

		static uint32_t /*remain_systick_us_prev = 0,*/ remain_systick_ms_prev = 0;
		// uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
		// remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

		uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
		remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

		system_time_ms += diff_ms;
		platform_watchdog_reset();

		fb[511] += diff_ms;

		adc_track();
		usb_poll(diff_ms);
		// UART3_ProcessReceivedData();
	}
}

void usbd_cdc_rx(const uint8_t *data, uint32_t size)
{
	usbd_cdc_unlock();
	console_cb((const char *)data, size);
}
