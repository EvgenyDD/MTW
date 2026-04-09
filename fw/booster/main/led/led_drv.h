#ifndef LED_DRV_H
#define LED_DRV_H

#include "led_hw.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	LED_MODE_OFF = 0,
	LED_MODE_ON,

	LED_MODE_FLASH_10HZ,	// 10 Hz [50 ms ON + 50 ms OFF]
	LED_MODE_FLASH_2_5HZ,	// 2.5 Hz [200 ms ON + 200 ms OFF]
	LED_MODE_FLASH_SINGLE,	// [200 ms ON + 1000 ms OFF]
	LED_MODE_FLASH_DOUBLE,	// [200 ms ON + 200 ms OFF + 200 ms ON + 1000 ms OFF]
	LED_MODE_FLASH_TRIPLE,	// [200 ms ON + 200 ms OFF + 200 ms ON + 200 ms OFF + 200 ms ON + 1000 ms OFF]
	LED_MODE_SFLASH_2_5HZ,	// SMOOTH 2.5 Hz [200 ms ON + 200 ms OFF]
	LED_MODE_SFLASH_SINGLE, // SMOOTH [200 ms ON + 1000 ms OFF]
	LED_MODE_SFLASH_DOUBLE, // SMOOTH [200 ms ON + 200 ms OFF + 200 ms ON + 1000 ms OFF]
	LED_MODE_SFLASH_TRIPLE, // SMOOTH [200 ms ON + 200 ms OFF + 200 ms ON + 200 ms OFF + 200 ms ON + 1000 ms OFF]
	LED_MODE_STROB_05HZ,	// 0.5 Hz [2 ms ON + 1998 ms OFF]
	LED_MODE_STROB_1HZ,		// 1 Hz [2 ms ON + 998 ms OFF]
	LED_MODE_STROB_2HZ,		// 2 Hz [2 ms ON + 498 ms OFF]
	LED_MODE_STROB_5HZ,		// 4 Hz [2 ms ON + 198 ms OFF]

	LED_MODE_MANUAL,

	LED_MODE_SIZE
} LED_MODE;

void led_drv_poll(uint32_t diff_ms);

void led_drv_set_led(uint32_t led_id, LED_MODE mode);
void led_drv_set_led_manual(uint32_t led_id, float brightness);

float interval_hit(int32_t value, int32_t middle, int32_t half_sector, int32_t range);

// demo
int led_startup_sample(void);
void led_startup_restart(void);

#endif // LED_DRV_H