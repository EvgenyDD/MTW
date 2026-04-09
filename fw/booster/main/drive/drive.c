#include "drive.h"

void drive_init(void)
{
	gpio_ll_input_disable(&GPIO, PIN_EN0);
	gpio_ll_input_disable(&GPIO, PIN_EN1);
	gpio_ll_input_disable(&GPIO, PIN_LVL0);
	gpio_ll_input_disable(&GPIO, PIN_LVL1);

	gpio_ll_output_enable(&GPIO, PIN_EN0);
	gpio_ll_output_enable(&GPIO, PIN_EN1);
	gpio_ll_output_enable(&GPIO, PIN_LVL0);
	gpio_ll_output_enable(&GPIO, PIN_LVL1);

	PIN_L(PIN_EN0);
	PIN_L(PIN_EN1);
	PIN_L(PIN_LVL0);
	PIN_L(PIN_LVL1);

	drive_timer_config();
}

void drv_manual(bool on0, bool on1, bool state0, bool state1)
{
	PIN_WR(PIN_EN0, on0);
	PIN_WR(PIN_EN1, on1);
	PIN_WR(PIN_LVL0, state0);
	PIN_WR(PIN_LVL1, state1);
}