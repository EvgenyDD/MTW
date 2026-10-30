#include "ui.h"
#include "conf.h"
#include "dcc.h"
#include "dcc_fb.h"
#include "debounce.h"
#include "driver/gpio.h"
#include "ina226.h"
#include "led_drv.h"
#include "safety.h"
#include "sh1107.h"

debounce_t btn[3];

void ui_init(void)
{
	debounce_init(&btn[0], 1200);
	debounce_init(&btn[1], 1200);
	debounce_init(&btn[2], 1200);
}

void ui_poll(uint32_t diff_ms)
{
	bool upd = false;
	static uint16_t add_op_cnt = 0;
	static int intrvl = 10;

	debounce_update(&btn[0], !gpio_get_level(GPIO_NUM_34), diff_ms);
	debounce_update(&btn[1], !gpio_get_level(GPIO_NUM_39), diff_ms);
	debounce_update(&btn[2], !gpio_get_level(GPIO_NUM_36), diff_ms);

	if(btn[0].state == BTN_PRESS_SHOT)
	{
		upd = true;
		safety_drive_enable(dcc_is_enabled() ? false : true);
	}
	if(btn[0].state == BTN_PRESS_LONG_SHOT)
	{
		upd = true;
		safety_reset_error();
	}
	if(btn[1].state == BTN_PRESS_SHOT)
	{
		upd = true;
		static uint8_t v = 0;
		v++;
		// dcc_add_op();
		static uint8_t fm = 0;
		if(++fm >= 3) fm = 0;
		// dcc_cd_cmd_speed_128(0, 0, 0);
		dcc_cd_cmd_analog_func(1, 0, fm);
		add_op_cnt++;
	}
	if(btn[2].state == BTN_PRESS_SHOT)
	{
		if(intrvl == 10)
			intrvl = 1;
		else
			intrvl = 10;
	}

	if(ina226_is_alert() && ina226.cur < LIMIT_OC) ina226_reset_alert();
	safety_set_error(ERR_OC, ina226_is_alert(), 0);
	safety_set_error(ERR_HW, ina226.err > 0, ina226.err);
	if(ina226.ready)
	{
		safety_set_error(ERR_OV, ina226.volt > LIMIT_OV, ina226.volt);
		safety_set_error(ERR_UV, ina226.volt < LIMIT_UV, ina226.volt);
	}

	led_drv_set_led(LED_0_GRN, dcc_is_enabled() ? LED_MODE_ON : LED_MODE_OFF);
	led_drv_set_led(LED_3_RED, safety_get_error_latched() ? LED_MODE_STROB_5HZ : LED_MODE_OFF);

	static uint32_t tmr2 = 0;
	tmr2 += diff_ms;
	if(tmr2 >= 10 && dcc_is_enabled())
	{
		tmr2 = 0;
		// dcc_add_op();
		// add_op_cnt++;
	}

	static uint32_t tmr = 0;
	tmr += diff_ms;
	if(tmr >= 100 || upd)
	{
		tmr = 0;

		// sh1107_dvd();

#define H 127 - 8

		static uint32_t pr = 0;
		if(++pr >= intrvl)
		{
			pr = 0;
			if(dcc_is_enabled())
			{
				// dcc_add_op();
				// add_op_cnt++;
			}
		}

		extern uint32_t g_proto_cnt[16];
		extern float err_reg[16];
		extern uint8_t last_ptrn[6];
		extern uint8_t last_ptrn_p;

		sh1107_string(0, H, F(font5x8), false, "E x%lx ", safety_get_error_latched());
		sh1107_string(32, H, F(font5x8), dcc_is_enabled(), "%s ", dcc_is_enabled() ? "ON" : "OFF");
		sh1107_string(0, H - 8 * 1, F(font5x8), false, "%.3f V ", ina226.volt);
		sh1107_string(0, H - 8 * 2, F(font5x8), false, "%.3f A ", ina226.cur);

		sh1107_string(0, H - 8 * 3, F(font5x8), false, "Al %d %ld ", ina226_is_alert(), ina226_get_alert_count());

		sh1107_string(0, H - 8 * 4, F(font5x8), false, "Snd %d ", add_op_cnt);
		sh1107_string(0, H - 8 * 5, F(font5x8), false, "Rx %ld ", dcc_dc_cb_get_rx());

		sh1107_string(0, H - 8 * 6, F(font5x8), false, " %ld %.3f", g_proto_cnt[0], err_reg[0]);
		sh1107_string(0, H - 8 * 7, F(font5x8), false, " %ld %.3f", g_proto_cnt[1], err_reg[1]);
		sh1107_string(0, H - 8 * 8, F(font5x8), false, " %ld %.3f", g_proto_cnt[2], err_reg[2]);
		sh1107_string(0, H - 8 * 9, F(font5x8), false, " %ld %.3f", g_proto_cnt[3], err_reg[3]);

		uint8_t p = last_ptrn_p;
		for(uint32_t i = 0; i < 6; i++)
		{
			// 	rx_buf[rx_ptr_head] = data[i];
			// 	rx_ptr_head = (rx_ptr_head + 1) % RX_BUF_SZ;
			sh1107_string(0, H - 81 - (6 * (5 - i)), F(font3x5), false, "x%x ", last_ptrn[p]);
			p = (p + 1) % 6;
		}

		sh1107_set_update_pending();
	}
}