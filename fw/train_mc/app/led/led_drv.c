#include "led_drv.h"
#include "i2c_common.h"
#include "platform.h"
#include <math.h>
#include <string.h>

// note: LED_PWM_QUANTS=50 ang gamma=2.8 have led_pwm_lvl[led] = 1 to brightness from 0.001 to 0.28 !

#define LED_PWM_QUANTS 256
#define half_period_flash 150

typedef struct
{
	LED_MODE mode;
	float brightness;
} led_t;

static led_t leds[LED_COUNT] = {0};
static uint8_t led_pwm_lvl[LED_COUNT] = {0};
static uint32_t counter_2000 = 0;
static uint32_t current_led = 0;

/**
 * @brief Interval hit linear function
 *          /\
 *         /  \
 * ......./    \...... output
 *
 * @param value positive or zero value
 * @param middle positive value
 * @param half_sector positive value
 * @param range positive value
 * @return float
 */
float interval_hit(int32_t value, int32_t middle, int32_t half_sector, int32_t range)
{
	const int32_t max = middle + half_sector;
	const int32_t min = middle - half_sector;

	if(max >= range)
	{
		if(value >= min && value < range)
			return 1.0f - fabsf((float)(value - middle) / (float)half_sector);
		else if(value < max - range)
			return 1.0f - fabsf((float)(value - middle + range) / (float)half_sector);
		else
			return 0;
	}
	else if(min < 0)
	{
		if(value >= 0 && value < max)
			return 1.0f - fabsf((float)(value - middle) / (float)half_sector);
		else if(value >= min + range)
			return 1.0f - fabsf((float)(value - middle - range) / (float)half_sector);
		else
			return 0;
	}
	else
	{
		if(value >= min && value < max)
			return 1.0f - fabsf((float)(value - middle) / (float)half_sector);
		else
			return 0;
	}
}

typedef void (*f)(void);

static void _led_prc_f10(void) { leds[current_led].brightness = (counter_2000 % 100) <= 50 ? 1.0f : 0; }
static void _led_prc_f2_5(void) { leds[current_led].brightness = (counter_2000 % 400) > 50 && (counter_2000 % 400) <= 200 ? 1.0f : 0; }

static void _led_prc_strb_05(void) { leds[current_led].brightness = (counter_2000) <= 2 ? 1.0f : 0; }
static void _led_prc_strb_1(void) { leds[current_led].brightness = (counter_2000 % 1000) <= 2 ? 1.0f : 0; }
static void _led_prc_strb_2(void) { leds[current_led].brightness = (counter_2000 % 500) > 50 && (counter_2000 % 500) <= 52 ? 1.0f : 0; }
static void _led_prc_strb_5(void) { leds[current_led].brightness = (counter_2000 % 200) <= 2 ? 1.0f : 0; }

static void _led_prc_f1(void) { leds[current_led].brightness = counter_2000 <= 200 ? 1.0f : 0; }
static void _led_prc_f2(void) { leds[current_led].brightness = counter_2000 < 200 || (counter_2000 >= 400 && counter_2000 < 600) ? 1.0f : 0; }
static void _led_prc_f3(void) { leds[current_led].brightness = counter_2000 < 200 || (counter_2000 >= 400 && counter_2000 < 600) || (counter_2000 >= 800 && counter_2000 < 1000) ? 1.0f : 0; }

static void _led_prc_sf2_5(void) { leds[current_led].brightness = interval_hit((int32_t)(counter_2000 % 400), 100, half_period_flash, 2000); }
static void _led_prc_sf1(void) { leds[current_led].brightness = interval_hit((int32_t)counter_2000, 100, half_period_flash, 2000); }
static void _led_prc_sf2(void)
{
	float y1 = interval_hit((int32_t)counter_2000, 100, half_period_flash, 2000);
	float y2 = interval_hit((int32_t)counter_2000, 500, half_period_flash, 2000);
	leds[current_led].brightness = y1 > 0 ? y1 : y2;
}
static void _led_prc_sf3(void)
{
	float y1 = interval_hit((int32_t)counter_2000, 100, half_period_flash, 2000),
		  y2 = interval_hit((int32_t)counter_2000, 500, half_period_flash, 2000),
		  y3 = interval_hit((int32_t)counter_2000, 900, half_period_flash, 2000);
	leds[current_led].brightness = y1 > 0 ? y1 : y2 > 0 ? y2
														: y3;
}

void (*led_processors[LED_MODE_SIZE])(void) = {
	NULL,			  // LED_MODE_OFF
	NULL,			  // LED_MODE_ON
	_led_prc_f10,	  // LED_MODE_FLASH_10HZ
	_led_prc_f2_5,	  // LED_MODE_FLASH_2_5HZ
	_led_prc_f1,	  // LED_MODE_FLASH_SINGLE
	_led_prc_f2,	  // LED_MODE_FLASH_DOUBLE
	_led_prc_f3,	  // LED_MODE_FLASH_TRIPLE
	_led_prc_sf2_5,	  // LED_MODE_SFLASH_2_5HZ
	_led_prc_sf1,	  // LED_MODE_SFLASH_SINGLE
	_led_prc_sf2,	  // LED_MODE_SFLASH_DOUBLE
	_led_prc_sf3,	  // LED_MODE_SFLASH_TRIPLE
	_led_prc_strb_05, // LED_MODE_STROB_05HZ
	_led_prc_strb_1,  // LED_MODE_STROB_1HZ
	_led_prc_strb_2,  // LED_MODE_STROB_2HZ
	_led_prc_strb_5,  // LED_MODE_STROB_5HZ
	NULL,			  // LED_MODE_MANUAL
};

void led_drv_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; // LED_BTM
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	i2c_mem_write(0xB6, 0x11, false, (uint8_t[]){0x3}, 1);
	i2c_mem_write(0xB6, 0x12, false, (uint8_t[]){0}, 1);
	i2c_mem_write(0xB6, 0x13, false, (uint8_t[]){0}, 1);
}

/**
 * @brief LED Driver Callback
 *
 * @param diff_ms
 * @return uint16_t output word to shift register
 */
void led_drv_poll(uint32_t diff_ms)
{
	// auto-reload counter
	counter_2000 += diff_ms;
	if(counter_2000 >= 2000) counter_2000 = 0;

	// effect processor
	for(current_led = 0; current_led < LED_COUNT; current_led++)
	{
		if(led_processors[leds[current_led].mode])
			led_processors[leds[current_led].mode]();

		// PWM calc
		if(leds[current_led].brightness <= 0.0f || leds[current_led].brightness > 1.0f)
		{
			led_pwm_lvl[current_led] = 0;
		}
		// else if(leds[current_led].brightness >= 1.0f)
		// {
		//     led_pwm_lvl[current_led] = LED_PWM_QUANTS;
		// }
		else
		{
			int idx = leds[current_led].brightness * (float)LED_PWM_QUANTS;

			static const uint8_t led_gamma[LED_PWM_QUANTS + 1 /* brightness == 1.0 */] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
				2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
				5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
				10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
				17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
				25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
				37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
				51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
				69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
				90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
				115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
				144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
				177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
				215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255, 255};

			led_pwm_lvl[current_led] = led_gamma[idx];
		}
	}

	// PWM processor
	static uint32_t cnt = 0;
	cnt++;
	if(cnt >= LED_PWM_QUANTS) cnt = 0;

	LED_HW_DRIVE();
}

void led_drv_set_led(uint32_t led_id, LED_MODE mode)
{
	leds[led_id].mode = mode;

	if(mode == LED_MODE_OFF || mode == LED_MODE_ON)
	{
		leds[led_id].brightness = mode == LED_MODE_ON ? 1.0f : 0;
	}
}

void led_drv_set_led_manual(uint32_t led_id, float brightness)
{
	leds[led_id].mode = LED_MODE_MANUAL;
	leds[led_id].brightness = brightness > 1.0f ? 1.0 : (brightness < 0 ? 0 : brightness);
}