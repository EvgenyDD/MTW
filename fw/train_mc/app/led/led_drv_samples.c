
#include "led_drv.h"
#include <math.h>

static uint16_t test_progress_counter = 0;

void led_startup_restart(void) { test_progress_counter = 0; }

/**
 * @brief Test
 *
 */
int led_startup_sample(void)
{
	const int32_t cnt_light_half_width = 300; // ms.

	const int32_t middles[LED_COUNT] = {
		100,
		200,
		300,
		400,
		500,
		600,
		700,
		800};

	static uint32_t time_ms_prev = 0;

	if(test_progress_counter > 1000) return 1;

	uint32_t time_ms_now = system_time_ms;

	if(time_ms_now != time_ms_prev)
	{
		test_progress_counter++;
		time_ms_prev = time_ms_now;
	}

	for(uint32_t i = 0; i < LED_COUNT; i++)
	{
		led_drv_set_led_manual(i, interval_hit(test_progress_counter, middles[i], cnt_light_half_width, 100000));
	}
	return 0;
}