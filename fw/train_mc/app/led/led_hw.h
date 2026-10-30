#ifndef LED_HW_H__
#define LED_HW_H__

enum LEDS
{
	LED_HEADLIGHT_FRONT = 0,
	LED_HEADLIGHT_REAR,
	LED_MARK_FRONT_L_WH,
	LED_MARK_FRONT_L_RD,
	LED_MARK_FRONT_R_WH,
	LED_MARK_FRONT_R_RD,
	LED_MARK_REAR_L_WH,
	LED_MARK_REAR_L_RD,
	LED_MARK_REAR_R_WH,
	LED_MARK_REAR_R_RD,
	LED_CAB_L,
	LED_CAB_R,
	LED_GEAR,
	LED_COUNT
};

#define LED_HW_DRIVE()                                                                   \
	cnt < led_pwm_lvl[LED_GEAR] ? (GPIOB->BSRRL = (1 << 3)) : (GPIOB->BSRRH = (1 << 3)); \
	if(diff_ms > 0)                                                                      \
	{                                                                                    \
		uint8_t b[] = {                                                                  \
			led_pwm_lvl[LED_MARK_FRONT_L_RD],                                            \
			led_pwm_lvl[LED_MARK_FRONT_R_RD],                                            \
			led_pwm_lvl[LED_HEADLIGHT_REAR],                                             \
			led_pwm_lvl[LED_MARK_FRONT_R_WH],                                            \
			led_pwm_lvl[LED_CAB_L],                                                      \
			led_pwm_lvl[LED_MARK_REAR_R_WH],                                             \
			led_pwm_lvl[LED_CAB_R],                                                      \
			led_pwm_lvl[LED_MARK_FRONT_L_WH],                                            \
			led_pwm_lvl[LED_HEADLIGHT_FRONT],                                            \
			led_pwm_lvl[LED_MARK_REAR_L_WH],                                             \
			led_pwm_lvl[LED_MARK_REAR_L_RD],                                             \
			led_pwm_lvl[LED_MARK_REAR_R_RD],                                             \
		};                                                                               \
		i2c_mem_write(0xB6, 0x20, false, b, sizeof(b));                                  \
	}

#endif // LED_HW_H__