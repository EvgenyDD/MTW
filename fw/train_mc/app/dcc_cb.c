#include "dcc.h"

#include "led_drv.h"

void dcc_dc_cb_spd_128(uint8_t code)
{
	led_drv_set_led(LED_HEADLIGHT_FRONT, LED_MODE_FLASH_10HZ);
}

void dcc_dc_cb_an_func(uint8_t addr, uint8_t value)
{
	led_drv_set_led(LED_HEADLIGHT_REAR, value);
}

void dcc_dc_cb_spd28_rev(uint8_t value)
{
	led_drv_set_led(LED_CAB_L, LED_MODE_FLASH_10HZ);
}

void dcc_dc_cb_spd28_fwd(uint8_t value)
{
	led_drv_set_led(LED_CAB_R, LED_MODE_FLASH_10HZ);
}
