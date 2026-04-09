#ifndef LED_HW_H__
#define LED_HW_H__

#include "driver/gpio.h"
#include "hal/gpio_ll.h"

#define PIN_H(X) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << X))
#define PIN_L(X) REG_WRITE(GPIO_OUT_W1TC_REG, (1 << X))

enum LEDS
{
	LED_0_GRN = 0,
	LED_1_BLU,
	LED_2_WHT,
	LED_3_RED,
	LED_COUNT
};

#define LED_HW_DRIVE()               \
	if(cnt < led_pwm_lvl[LED_0_GRN]) \
	{                                \
		PIN_H(15);                   \
	}                                \
	else                             \
	{                                \
		PIN_L(15);                   \
	}                                \
	if(cnt < led_pwm_lvl[LED_1_BLU]) \
	{                                \
		PIN_H(4);                    \
	}                                \
	else                             \
	{                                \
		PIN_L(4);                    \
	}                                \
	if(cnt < led_pwm_lvl[LED_2_WHT]) \
	{                                \
		PIN_H(12);                   \
	}                                \
	else                             \
	{                                \
		PIN_L(12);                   \
	}                                \
	if(cnt < led_pwm_lvl[LED_3_RED]) \
	{                                \
		PIN_H(13);                   \
	}                                \
	else                             \
	{                                \
		PIN_L(13);                   \
	}

#endif // LED_HW_H__