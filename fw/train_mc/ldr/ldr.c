#include "config_system.h"
#include "crc.h"
#include "fw_header.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"
#include "usb_hw.h"

#define BOOT_DELAY 500

#define SYSTICK_IN_US (168000000 / 1000000)
#define SYSTICK_IN_MS (168000000 / 1000)

bool g_stay_in_boot = false;
uint32_t g_uid[3];

static volatile uint32_t boot_delay = BOOT_DELAY;
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
	platform_init_loader();

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_BOOTLOADER); // let preboot know it was booted from bootloader

	if(ret_mem_is_bl_stuck()) g_stay_in_boot = true;

	usb_init();

	prof_mark(&prev_systick);
	for(;;)
	{
		uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

		static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
		uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
		remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

		uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
		remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

		platform_watchdog_reset();

		if(!boot_delay &&
		   !g_stay_in_boot &&
		   g_fw_info[FW_APP].locked == false)
		{
			platform_reset();
		}

		boot_delay = boot_delay >= diff_ms ? boot_delay - diff_ms : 0;

		usb_poll(diff_ms);

		platform_reset();
	}
}
