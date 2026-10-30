#include "adc.h"
#include "cobs.h"
#include "config_system.h"
#include "console.h"
#include "crc.h"
#include "dcc.h"
#include "drive.h"
#include "fw_header.h"
#include "graph.h"
#include "i2c_common.h"
#include "led_drv.h"
#include "platform.h"
#include "prof.h"
#include "proto.h"
#include "ret_mem.h"
#include "usb_hw.h"
#include "usbd_core_cdc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool g_stay_in_boot = false;
uint32_t g_uid[3];
volatile uint32_t system_time_ms = 0;
uint8_t g_dcc_self_id = DCC_SELF_ID;

static int32_t prev_systick = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
config_entry_t g_device_config[] = {
	{"dcc_id", sizeof(g_dcc_self_id), 0, &g_dcc_self_id},
	{"cal_hall", 2 * sizeof(hall_calib_t), 0, &ctrl.hall_cal[0]},
	{"cal_dir", sizeof(g_dir), 0, &g_dir},
	{"cal_pp", sizeof(ctrl.pp), 0, &ctrl.pp},
	{"cal_off", sizeof(ctrl.hall), 0, &ctrl.pp},
};
#pragma GCC diagnostic pop
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

uint8_t arr_i2c[256] = {0};

void main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from app

	drv_init();
	dcc_hw_init();
	usb_init();

	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_12; // DBG
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7; // DBG
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	int err = config_validate();
	if(err == CONFIG_STS_OK) config_read_storage();

	i2c_init();
	led_drv_init();

	led_drv_set_led(LED_MARK_FRONT_L_RD, LED_MODE_SFLASH_SINGLE);
	led_drv_set_led(LED_MARK_FRONT_R_RD, LED_MODE_SFLASH_SINGLE);
	led_drv_set_led(LED_MARK_REAR_L_RD, LED_MODE_SFLASH_SINGLE);
	led_drv_set_led(LED_MARK_REAR_R_RD, LED_MODE_SFLASH_SINGLE);

	led_drv_set_led_manual(LED_HEADLIGHT_FRONT, LED_BR_MIN);
	led_drv_set_led_manual(LED_HEADLIGHT_REAR, LED_BR_MIN);

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
		dcc_dc_poll();

		led_drv_poll(diff_ms);

#if 0
		static uint32_t t1 = 0, t2 = 0;
		t1 += diff_ms;
		if(t1 >= 1500)
		{
			t1 = 0;
#if 1
			t2++;
			if(t2 >= 13)
			{
				t2 = 0;
				for(uint32_t i = 0; i < 12; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){1}, 1);
				}
			}
			{
				if(t2 > 0) i2c_mem_write(0xB6, 0x20 - 1 + t2, false, (uint8_t[]){255}, 1);
				if(t2 > 1) i2c_mem_write(0xB6, 0x20 - 2 + t2, false, (uint8_t[]){1}, 1);
			}
#else
			t2++;
			if(t2 >= 2)
			{
				t2 = 0;
			}
			if(t2 == 0)
			{
				for(uint32_t i = 2; i < 10; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){100}, 1);
				}

				for(uint32_t i = 0; i < 2; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){1}, 1);
				}
				for(uint32_t i = 10; i < 12; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){1}, 1);
				}
			}
			else
			{
				for(uint32_t i = 2; i < 10; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){0}, 1);
				}

				for(uint32_t i = 0; i < 2; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){250}, 1);
				}
				for(uint32_t i = 10; i < 12; i++)
				{
					i2c_mem_write(0xB6, 0x20 + i, false, (uint8_t[]){250}, 1);
				}
			}
#endif
		}
#endif
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
				drv_set_ol(o.iq, o.id, o.pos, o.vel, o.vel_rate);
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
		in.i.i[0] = ctrl.foc.iq_fb_filt * 1000;
		in.i.i[1] = ctrl.foc.id_fb_filt * 1000;
		in.i.pwm[0] = TIM1->CCR1;
		in.i.pwm[1] = TIM1->CCR2;
		in.i.pwm[2] = TIM1->CCR3;
		in.i.temp[0] = CLIPS(g_adc_val.tmtr, 127);
		in.i.temp[1] = CLIPS(g_adc_val.tinv, 127);
		in.i.temp[2] = CLIPS(g_adc_val.tcpu, 127);
		in.i.pos = ctrl.angle_enc;
		in.i.vel = ctrl.rpm;
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