#include "adc.h"
#include "cobs.h"
#include "config_system.h"
#include "console.h"
#include "crc.h"
#include "drive.h"
#include "fw_header.h"
#include "graph.h"
#include "platform.h"
#include "prof.h"
#include "proto.h"
#include "ret_mem.h"
#include "usb_hw.h"
#include "usbd_core_cdc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSTICK_IN_US (168000000 / 1000000)
#define SYSTICK_IN_MS (168000000 / 1000)

bool g_stay_in_boot = false;
uint32_t g_uid[3];

volatile uint32_t system_time_ms = 0;
static int32_t prev_systick = 0;

uint8_t txBuffer[256] = "abcd"; // Larger buffer for variable size data
volatile uint8_t dmaBusy = 0;	// Flag to indicate DMA is busy

static uint8_t dummy = 0;

config_entry_t g_device_config[] = {
	{"dummy", sizeof(dummy), 0, &dummy},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

void delay_ms(volatile uint32_t delay_ms)
{
	volatile uint32_t start = 0;
	int32_t mark_prev = 0;
	prof_mark(&mark_prev);
	const uint32_t time_limit = delay_ms * SYSTICK_IN_MS;
	for(;;)
	{
		start += (uint32_t)prof_mark(&mark_prev);
		if(start >= time_limit) return;
	}
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

void EXTI4_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line4) != RESET)
	{
		TIM_Cmd(TIM3, DISABLE);
		TIM_SetCounter(TIM3, 0);
		TIM_Cmd(TIM3, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line4);
	}
}

void EXTI9_5_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line5) != RESET)
	{
		TIM_Cmd(TIM3, DISABLE);
		TIM_SetCounter(TIM3, 0);
		TIM_Cmd(TIM3, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line5);
	}
}

static uint8_t text[16] = {"012"};
void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		TIM_Cmd(TIM3, DISABLE);
		if((GPIOA->IDR & (1 << 4)) == 0 &&
		   (GPIOA->IDR & (1 << 5)) == 0)
		{
			// if(++text[0] > '9') text[0] = '0';
			trigger_send(text, 3);
		}
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}

void DMA1_Stream3_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_Stream3, DMA_IT_TCIF3))
	{
		DMA_ClearITPendingBit(DMA1_Stream3, DMA_IT_TCIF3);
	}
}

void main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	drv_init();

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from app

	usb_init();

	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

		GPIO_InitTypeDef GPIO_InitStruct;
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_Init(GPIOA, &GPIO_InitStruct);

		SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource4);
		SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource5);

		EXTI_InitTypeDef EXTI_InitStruct;
		EXTI_InitStruct.EXTI_Line = EXTI_Line4;
		EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
		EXTI_InitStruct.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStruct);

		EXTI_InitStruct.EXTI_Line = EXTI_Line5;
		EXTI_Init(&EXTI_InitStruct);

		NVIC_InitTypeDef NVIC_InitStruct;
		NVIC_InitStruct.NVIC_IRQChannel = EXTI4_IRQn;
		NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
		NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStruct);

		NVIC_InitStruct.NVIC_IRQChannel = EXTI9_5_IRQn;
		NVIC_Init(&NVIC_InitStruct);

		TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
		TIM_TimeBaseStruct.TIM_Prescaler = 84 - 1;
		TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
		TIM_TimeBaseStruct.TIM_Period = 20 - 1;
		TIM_TimeBaseStruct.TIM_ClockDivision = 0;
		TIM_TimeBaseStruct.TIM_RepetitionCounter = 0;
		TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);

		TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

		NVIC_InitStruct.NVIC_IRQChannel = TIM3_IRQn;
		NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
		NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStruct);

		TIM_Cmd(TIM3, DISABLE);

		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
		// GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
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
		DMA_InitStructure.DMA_Channel = DMA_Channel_4; // USART3_TX uses channel 4
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
	}

	prof_mark(&prev_systick);
	for(;;)
	{
		// time diff
		uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

		static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
		// uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
		remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

		uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
		remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

		system_time_ms += diff_ms;

		platform_watchdog_reset();

		usb_poll(diff_ms);
	}
}

uint8_t ret_buf[256];
uint8_t cobs_buffer[256];
uint8_t gnrc_buffer[256];
uint32_t cobs_buffer_size = 0;
uint32_t gnrc_buffer_size = 0;
static bool cobs_ongoing = false;
uint8_t tx_cobs_buffer[256] = {0xFF};
uint8_t rx_cobs_buffer[256];
bool cobs_error = false;

static void cobs_encode_tx(uint8_t *data, uint32_t size)
{
	uint32_t tx = cobs_encode(data, size, &tx_cobs_buffer[1]);
	if(2 + tx > sizeof(tx_cobs_buffer)) return;
	tx_cobs_buffer[1 + tx] = 0xFF;
	usbd_cdc_push_data(tx_cobs_buffer, 2 + tx);
}

static void process_cobs(uint8_t *data, uint32_t size)
{
	switch(data[0])
	{
	case PROTO_CLOOP:
	{
		drive_reset_master_to();
		drv_out_t o;
		if(size - 1 == sizeof(drv_out_t))
		{
			memcpy(&o, &data[1], sizeof(drv_out_t));
			if(o.mode == DRV_MODE_ERROR)
				drv_err_rst();
			else if(o.mode == DRV_MODE_OFF)
				drv_set_off();
			else if(o.mode == DRV_MODE_OL)
				drv_set_ol(o.iq, o.pos, o.vel, o.vel_rate);
			else if(o.mode == DRV_MODE_CURRENT)
				drv_set_cur(o.iq, o.id);
			else if(o.mode == DRV_MODE_VELOCITY)
				drv_set_vel(o.vel, o.vel_rate);
		}
		struct __attribute__((packed))
		{
			uint8_t b;
			drv_in_t i;
		} in = {PROTO_CLOOP, {0}};
		in.i.err_mask = ctrl.err;
		in.i.mode = ctrl.mode;
		in.i.ub = g_adc_val.ub * 100;
		in.i.i[0] = ctrl.foc.iq_fb * 1000;
		in.i.i[1] = ctrl.foc.id_fb * 1000;
		in.i.pwm[0] = TIM1->CCR1;
		in.i.pwm[1] = TIM1->CCR2;
		in.i.pwm[2] = TIM1->CCR3;
		in.i.temp[0] = CLIPS(g_adc_val.tmtr, 127);
		in.i.temp[1] = CLIPS(g_adc_val.tinv, 127);
		in.i.temp[2] = CLIPS(g_adc_val.tcpu, 127);
		cobs_encode_tx((uint8_t *)&in, sizeof(in));
		break;
	}

	case PROTO_GRAPH:
		if(size > 1)
		{
			gr_wr_cb(&data[1], size - 1);
		}
		else
		{
			size_t sz = sizeof(ret_buf) - 1;
			gr_rd_cb(&ret_buf[1], &sz);
			ret_buf[0] = PROTO_GRAPH;
			if(sz != 0) cobs_encode_tx(ret_buf, 1 + sz);
		}
		break;

	default:
		_console_print("unknwn cobs x%x\n", data[0]);
		break;
	}
}

void usbd_cdc_rx(const uint8_t *data, uint32_t size)
{
	uint32_t it = 0;
	if(cobs_ongoing)
	{
	PRC_COBS_IN:
		for(; it < size; it++)
		{
			if(data[it] == 0xFF)
			{
				if(!cobs_error && cobs_buffer_size > 1)
				{
					uint32_t sz = cobs_decode(cobs_buffer, cobs_buffer_size, rx_cobs_buffer);
					process_cobs(rx_cobs_buffer, sz);
				}
				cobs_buffer_size = 0;
				cobs_ongoing = false;
				it++;
				goto PRC_GNRC_IN;
			}
			else
			{
				cobs_buffer[cobs_buffer_size++] = data[it];
				if(cobs_buffer_size >= sizeof(cobs_buffer))
				{
					cobs_error = true;
					cobs_buffer_size = 0;
				}
			}
		}
	}
	else
	{
	PRC_GNRC_IN:
		for(; it < size; it++)
		{
			if(data[it] == 0xFF)
			{
				cobs_ongoing = true;
				cobs_buffer_size = 0;
				it++;
				goto PRC_COBS_IN;
			}
			else
			{
				gnrc_buffer[gnrc_buffer_size++] = data[it];
				if(gnrc_buffer_size >= sizeof(gnrc_buffer))
				{
					console_cb((const char *)gnrc_buffer, gnrc_buffer_size);
					gnrc_buffer_size = 0;
				}
			}
		}
		if(gnrc_buffer_size)
		{
			console_cb((const char *)gnrc_buffer, gnrc_buffer_size);
			gnrc_buffer_size = 0;
		}
	}
}

uint32_t platform_get_loop_freq(void)
{
	return DRV_FREQ;
}