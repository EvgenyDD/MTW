#include "adc.h"
#include "cobs.h"
#include "config_system.h"
#include "console.h"
#include "crc.h"
#include "drive_hal.h"
#include "error.h"
#include "fw_header.h"
#include "graph.h"
#include "platform.h"
#include "prof.h"
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

void main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	drive_init();

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from app

	usb_init();

	prof_mark(&prev_systick);
	for(;;)
	{
		// time diff
		uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

		static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
		uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
		remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

		uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
		remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

		system_time_ms += diff_ms;

		platform_watchdog_reset();

		usb_poll(diff_ms);
	}
}

uint8_t ret_buf[128];
uint8_t cobs_buffer[256];
uint8_t gnrc_buffer[256];
uint32_t cobs_buffer_size = 0;
uint32_t gnrc_buffer_size = 0;
static bool cobs_ongoing = false;
uint8_t tx_cobs_buffer[256] = {0xFF};
uint8_t rx_cobs_buffer[256];
bool cobs_error = false;

// static void cobs_encode_tx(uint8_t *data, uint32_t size)
// {
// 	uint32_t tx = cobs_encode(data, size, &tx_cobs_buffer[1]);
// 	if(2 + tx > sizeof(tx_cobs_buffer)) return;
// 	tx_cobs_buffer[1 + tx] = 0xFF;
// 	usbd_cdc_push_data(tx_cobs_buffer, 2 + tx);
// }

static void process_cobs(uint8_t *data, uint32_t size)
{
	switch(data[0])
	{
		// case PROTO_GRAPH_GET:
		// 	if(graph_is_enabled())
		// 	{
		// 		cobs_encode_tx((uint8_t[]){PROTO_GRAPH_GET}, 1);
		// 	}
		// 	else
		// 	{
		// 		proto_graph_hdr_t h;
		// 		h.cmd = PROTO_GRAPH_GET;
		// 		h.vars = graph_get_vars();
		// 		h.tot_size = graph_get_tot_size();
		// 		cobs_encode_tx((uint8_t *)&h, sizeof(h));
		// 	}
		// 	break;

		// case PROTO_GRAPH_GET_VAR:
		// 	if(size == 2 && !graph_is_enabled())
		// 	{
		// 		const graph_var_t *v = graph_get_var(data[1]);
		// 		if(v)
		// 		{
		// 			proto_graph_var_t var;
		// 			var.cmd = PROTO_GRAPH_GET_VAR;
		// 			var.scale = v->scale;
		// 			var.type = v->type;
		// 			strncpy(var.name, v->name, sizeof(var.name));
		// 			cobs_encode_tx((uint8_t *)&var, sizeof(var));
		// 			break;
		// 		}
		// 	}
		// 	cobs_encode_tx((uint8_t[]){PROTO_GRAPH_GET_VAR}, 1);
		// 	break;

		// case PROTO_GRAPH_GET_CHUNK:
		// 	if(size == 1 + 4 && !graph_is_enabled())
		// 	{
		// 		uint32_t offset;
		// 		memcpy(&offset, &data[1], 4);
		// 		if(offset < graph_get_tot_size())
		// 		{
		// 			uint32_t size_send = graph_get_tot_size() - offset;
		// 			if(size_send + 5 > 128) size_send = 128 - 5;
		// 			ret_buf[0] = PROTO_GRAPH_GET_CHUNK;
		// 			memcpy(&ret_buf[1], &offset, 4);
		// 			memcpy(&ret_buf[5], &(graph_get_array()[offset]), size_send);
		// 			cobs_encode_tx(ret_buf, 5 + size_send);
		// 			break;
		// 		}
		// 	}
		// 	cobs_encode_tx((uint8_t[]){PROTO_GRAPH_GET_CHUNK}, 1);
		// 	break;

	default: _console_print("unknwn cobs x%x\n", data[0]); break;
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