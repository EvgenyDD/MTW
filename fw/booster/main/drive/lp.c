#include "lp.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "hal/timer_hal.h"
#include "hal/timer_ll.h"
#include "ilda.h"
#include "lp_hw.h"
#include "proto.h"
#include "rom/gpio.h"
#include "sdkconfig.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/periph_defs.h"
#include "soc/soc_caps.h"
#include "soc/spi_reg.h"
#include "soc/timer_periph.h"
#include <math.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>
#include <soc/spi_struct.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

volatile uint32_t lpq_cnt = 1;
volatile uint32_t lpq_tail = 0;
volatile uint32_t lpq_head = 0;

volatile uint16_t lpq[6 * POINT_BUF_CNT] = {0};

#define DELAY                                  \
	for(volatile uint32_t i = 0; i < 100; i++) \
		asm("nop");

static uint8_t gamma_en = true;
static uint8_t full_power = false;

static void pins_config(bool enable)
{
	if(enable)
	{
		gpio_ll_input_disable(&GPIO, PIN_LDAC);
		gpio_ll_input_disable(&GPIO, PIN_COUNT);
		gpio_ll_output_enable(&GPIO, PIN_LDAC);
		gpio_ll_output_enable(&GPIO, PIN_COUNT);
		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_LDAC], PIN_FUNC_GPIO);
		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_COUNT], PIN_FUNC_GPIO);

		PIN_H(PIN_LDAC);
		PIN_L(PIN_COUNT);

		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SPI_DATA], PIN_FUNC_GPIO);
		gpio_ll_output_enable(&GPIO, PIN_SPI_DATA);
		gpio_matrix_out(PIN_SPI_DATA, HSPID_OUT_IDX, false, false);
		gpio_matrix_in(PIN_SPI_DATA, HSPID_IN_IDX, false);

		PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SPI_CLK], PIN_FUNC_GPIO);
		gpio_ll_output_enable(&GPIO, PIN_SPI_CLK);
		gpio_matrix_out(PIN_SPI_CLK, HSPICLK_OUT_IDX, false, false);
	}
	else
	{
		gpio_ll_output_disable(&GPIO, PIN_LDAC);
		gpio_ll_output_disable(&GPIO, PIN_COUNT);
		gpio_ll_output_disable(&GPIO, PIN_SPI_DATA);
		gpio_ll_output_disable(&GPIO, PIN_SPI_CLK);
		gpio_ll_input_enable(&GPIO, PIN_LDAC);
		gpio_ll_input_enable(&GPIO, PIN_COUNT);
		gpio_ll_input_enable(&GPIO, PIN_SPI_DATA);
		gpio_ll_input_enable(&GPIO, PIN_SPI_CLK);
		gpio_ll_pulldown_en(&GPIO, PIN_LDAC);
		gpio_ll_pullup_en(&GPIO, PIN_COUNT);
		gpio_ll_pulldown_en(&GPIO, PIN_SPI_DATA);
		gpio_ll_pulldown_en(&GPIO, PIN_SPI_CLK);
	}
}

void lp_init(void)
{
	spi_config();
	timer_config();

	memset((void *)lpq, 0, sizeof(lpq));

	for(uint32_t i = 0; i < 6; i++)
	{
		lpq[i * 6] = 5000; // us
	}

#define p_l 2048 - 300
#define p_h 2048 + 300

	lpq[6 * 0 + 1] = 0; // X
	lpq[6 * 0 + 2] = 0; // Y
	lpq[6 * 0 + 3] = 0; // G
	lpq[6 * 0 + 4] = 0; // B
	lpq[6 * 0 + 5] = 0; // R

	lpq[6 * 1 + 1] = 2048; // X
	lpq[6 * 1 + 2] = 2048; // Y
	lpq[6 * 1 + 3] = 4095; // G
	lpq[6 * 1 + 4] = 4095; // B
	lpq[6 * 1 + 5] = 4095; // R

	lp_off();

	// disable unused DAC channel
	PIN_L(PIN_LDAC);
	DELAY;
	PIN_H(PIN_LDAC);
	DELAY;
	for(uint32_t i = 0; i < 4; i++)
	{
		PIN_L(PIN_COUNT);
		DELAY;
		PIN_H(PIN_COUNT);
		DELAY;
	}
	DAC_SPI.data_buf[0] = (0 << (15 - 8)) | (0 << (12 - 8));
	DAC_SPI.cmd.usr = 1;
	while(DAC_SPI.cmd.usr)
		;
	PIN_H(PIN_COUNT);
	DELAY;
	PIN_L(PIN_LDAC);
	DELAY;
	PIN_H(PIN_LDAC);
	DELAY;
}

void lp_off(void)
{
	timer_ll_enable_counter(&TIMERG0, 0, false);
	for(volatile uint32_t i = 0; i < 10000; i++)
		asm("nop");
	manual_cmd((uint16_t[]){2048, 2048, 0, 0, 0});
	pins_config(false);
}

void lp_on(void)
{
	pins_config(true);
	PIN_H(PIN_COUNT);
	DELAY;
	PIN_L(PIN_LDAC);
	DELAY;
	PIN_H(PIN_LDAC);
	DELAY;
	timer_ll_enable_counter(&TIMERG0, 0, true);
	// timer_ll_set_alarm_value(&TIMERG0, 0, lpq[6 * lpq_head + 0]); // opt
	// uint64_t old_reload = timer_ll_get_reload_value(&TIMERG0, 0);
	// timer_ll_set_reload_value(&TIMERG0, 0, 0);
	// timer_ll_trigger_soft_reload(&TIMERG0, 0);
	// timer_ll_set_reload_value(&TIMERG0, 0, old_reload);
}

void lp_reset_q(void)
{
	lpq_cnt = 1;
	lpq_tail = 0;
	lpq_head = 0;
}

uint32_t lp_get_free_buf(void)
{
	return lpq_tail <= lpq_head
			   ? POINT_BUF_CNT - 1 - lpq_head + lpq_tail
			   : lpq_tail - lpq_head - 1;
}

void lp_append_point(uint16_t time_us, uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b)
{
	volatile uint32_t new_head = lpq_head + 1, new_cnt = lpq_cnt;
	if(new_head >= POINT_BUF_CNT)
	{
		new_head = 0;
	}
	else if(new_head >= new_cnt)
	{
		new_cnt++;
	}

	if(x > 4095) x = 4095;
	if(y > 4095) y = 4095;
	if(r > 4095) r = 4095;
	if(g > 4095) g = 4095;
	if(b > 4095) b = 4095;

	lpq[6 * new_head + 0] = time_us;
	lpq[6 * new_head + COORD_X] = 4095 - x;
	lpq[6 * new_head + COORD_Y] = 4095 - y;

	if(full_power && gamma_en)
	{
		lpq[6 * new_head + CLR_R] = map_u16(gamma_f[r], 0, LVL_MAX, R_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(gamma_f[g], 0, LVL_MAX, G_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(gamma_f[b], 0, LVL_MAX, B_MIN, LVL_MAX);
	}
	else if(full_power == false && gamma_en)
	{
		lpq[6 * new_head + CLR_R] = map_u16(gamma_f[r], 0, LVL_MAX, R_MIN, R_TC_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(gamma_f[g], 0, LVL_MAX, G_MIN, G_TC_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(gamma_f[b], 0, LVL_MAX, B_MIN, B_TC_MAX);
	}
	else if(full_power && gamma_en == false)
	{
		lpq[6 * new_head + CLR_R] = map_u16(r, 0, LVL_MAX, R_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(g, 0, LVL_MAX, G_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(b, 0, LVL_MAX, B_MIN, LVL_MAX);
	}
	else if(full_power == false && gamma_en == false)
	{
		lpq[6 * new_head + CLR_R] = map_u16(r, 0, LVL_MAX, R_MIN, R_TC_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(g, 0, LVL_MAX, G_MIN, G_TC_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(b, 0, LVL_MAX, B_MIN, B_TC_MAX);
	}

	lpq_cnt = new_cnt;
	lpq_head = new_head;
}

void lp_append_pointa(uint16_t *a)
{
	volatile uint32_t new_head = lpq_head + 1, new_cnt = lpq_cnt;
	if(new_head >= POINT_BUF_CNT)
	{
		new_head = 0;
	}
	else if(new_head >= new_cnt)
	{
		new_cnt++;
	}

	if(a[1] > 4095) a[1] = 4095;
	if(a[2] > 4095) a[2] = 4095;
	if(a[3] > 4095) a[3] = 4095;
	if(a[4] > 4095) a[4] = 4095;
	if(a[5] > 4095) a[5] = 4095;

	lpq[6 * new_head + 0] = a[0];
	lpq[6 * new_head + COORD_X] = 4095 - a[1];
	lpq[6 * new_head + COORD_Y] = 4095 - a[2];

	if(full_power && gamma_en)
	{
		lpq[6 * new_head + CLR_R] = map_u16(gamma_f[a[3]], 0, LVL_MAX, R_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(gamma_f[a[4]], 0, LVL_MAX, G_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(gamma_f[a[5]], 0, LVL_MAX, B_MIN, LVL_MAX);
	}
	else if(full_power == false && gamma_en)
	{
		lpq[6 * new_head + CLR_R] = map_u16(gamma_f[a[3]], 0, LVL_MAX, R_MIN, R_TC_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(gamma_f[a[4]], 0, LVL_MAX, G_MIN, G_TC_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(gamma_f[a[5]], 0, LVL_MAX, B_MIN, B_TC_MAX);
	}
	else if(full_power && gamma_en == false)
	{
		lpq[6 * new_head + CLR_R] = map_u16(a[3], 0, LVL_MAX, R_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(a[4], 0, LVL_MAX, G_MIN, LVL_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(a[5], 0, LVL_MAX, B_MIN, LVL_MAX);
	}
	else if(full_power == false && gamma_en == false)
	{
		lpq[6 * new_head + CLR_R] = map_u16(a[3], 0, LVL_MAX, R_MIN, R_TC_MAX);
		lpq[6 * new_head + CLR_G] = map_u16(a[4], 0, LVL_MAX, G_MIN, G_TC_MAX);
		lpq[6 * new_head + CLR_B] = map_u16(a[5], 0, LVL_MAX, B_MIN, B_TC_MAX);
	}

	lpq_cnt = new_cnt;
	lpq_head = new_head;
}
