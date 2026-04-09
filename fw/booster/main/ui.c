#include "ui.h"
#include "debounce.h"
#include "drive.h"
#include "driver/gpio.h"
#include "sh1107.h"

debounce_t btn[3];

static uint8_t p_item = 0;
static uint8_t act[4] = {0};

void ui_init(void)
{
	debounce_init(&btn[0], 2000);
	debounce_init(&btn[1], 2000);
	debounce_init(&btn[2], 2000);
}

void ui_poll(uint32_t diff_ms)
{
	bool upd = false;

	debounce_update(&btn[0], !gpio_get_level(GPIO_NUM_34), diff_ms);
	debounce_update(&btn[1], !gpio_get_level(GPIO_NUM_39), diff_ms);
	debounce_update(&btn[2], !gpio_get_level(GPIO_NUM_36), diff_ms);

	if(btn[0].state == BTN_PRESS_SHOT)
	{
		if(++p_item >= 4) p_item = 0;
		upd = true;
	}
	if(btn[1].state == BTN_PRESS_SHOT)
	{
		if(--p_item >= 4) p_item = 3;
		upd = true;
	}
	if(btn[2].state == BTN_PRESS_SHOT)
	{
		act[p_item] = act[p_item] == 0 ? 1 : 0;
		drv_manual(act[0], act[1], act[2], act[3]);
		upd = true;
	}

	static uint32_t cnt = 0;
	cnt += diff_ms;
	if(cnt >= 100 || upd)
	{
		cnt = 0;

		sh1107_dvd();
		sh1107_string(0, 27, F(font5x8), act[0], "%cEN0", p_item == 0 ? '>' : ' ');
		sh1107_string(0, 18, F(font5x8), act[1], "%cEN1", p_item == 1 ? '>' : ' ');
		sh1107_string(0, 9, F(font5x8), act[2], "%cLVL0", p_item == 2 ? '>' : ' ');
		sh1107_string(0, 0, F(font5x8), act[3], "%cLVL1", p_item == 3 ? '>' : ' ');
		sh1107_set_update_pending();
	}
}