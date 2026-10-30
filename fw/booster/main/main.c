#include "console.h"
#include "dcc.h"
#include "debounce.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/gpio_ll.h"
#include "ina226.h"
#include "led_drv.h"
#include "safety.h"
#include "sh1107.h"
#include "ui.h"
#include "web/time_wrapper.h"
#include "web/web_common.h"
#include "web/wifi.h"
#include "web/ws.h"

#if !CONFIG_IDF_TARGET_ESP32
#error "Wrong target"
#endif

// 4MB

uint32_t restart_timer = 0;
bool flashing = false;

void app_main(void)
{
	console_init();
	web_common_init();
	wifi_init();
	ws_init();
	time_wrapper_init(NULL);

	gpio_install_isr_service(0);
	uint32_t prev_systick = esp_log_timestamp();

	esp_rom_gpio_pad_select_gpio(4); // LEDs
	esp_rom_gpio_pad_select_gpio(12);
	esp_rom_gpio_pad_select_gpio(13);
	esp_rom_gpio_pad_select_gpio(15);

	gpio_set_direction(4, GPIO_MODE_OUTPUT);
	gpio_set_direction(12, GPIO_MODE_OUTPUT);
	gpio_set_direction(13, GPIO_MODE_OUTPUT);
	gpio_set_direction(15, GPIO_MODE_OUTPUT);

	sh1107_init();
	ina226_init();

	esp_rom_gpio_pad_select_gpio(34); // buttons
	esp_rom_gpio_pad_select_gpio(36);
	esp_rom_gpio_pad_select_gpio(39);
	gpio_set_direction(34, GPIO_MODE_INPUT);
	gpio_set_direction(36, GPIO_MODE_INPUT);
	gpio_set_direction(39, GPIO_MODE_INPUT);

	ui_init();

	dcc_hw_init();

	while(1)
	{
		const uint32_t systick_now = esp_log_timestamp();
		uint32_t diff_ms = systick_now - prev_systick;
		prev_systick = systick_now;

		if(restart_timer)
		{
			if(restart_timer <= diff_ms) esp_restart();
			restart_timer -= diff_ms;
		}

		sh1107_poll();
		ui_poll(diff_ms);
		led_drv_poll(diff_ms);
		wifi_poll(diff_ms);
		ina226_poll(diff_ms);
		safety_poll(diff_ms);

		if(flashing) safety_drive_enable(false);

		// vTaskDelay(1);
		taskYIELD();
	}
}
