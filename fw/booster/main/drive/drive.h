#ifndef DRIVE_H__
#define DRIVE_H__

#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_intr_alloc.h"
#include "hal/gpio_ll.h"
#include "hal/timer_ll.h"
#include "soc/periph_defs.h"
#include "soc/timer_periph.h"
#include <stdint.h>

#define PIN_EN0 22
#define PIN_EN1 19
#define PIN_LVL0 23
#define PIN_LVL1 21

#define PIN_H(X) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << X))
#define PIN_L(X) REG_WRITE(GPIO_OUT_W1TC_REG, (1 << X))
#define PIN_WR(X, s) \
	if(s)            \
	{                \
		PIN_H(X);    \
	}                \
	else             \
	{                \
		PIN_L(X);    \
	}

void drive_init(void);
void drv_manual(bool on0, bool on1, bool state0, bool state1);

extern void periph_module_enable(periph_module_t periph);

static inline void drive_timer_config(void)
{
	periph_module_enable(PERIPH_TIMG0_MODULE);
	timer_ll_set_reload_value(&TIMERG0, 0, 0);
	timer_ll_trigger_soft_reload(&TIMERG0, 0);
	timer_ll_set_clock_source(&TIMERG0, 0, SOC_MOD_CLK_APB);
	timer_ll_set_clock_prescale(&TIMERG0, 0, 80 /* 1 us per tick */);
	timer_ll_set_count_direction(&TIMERG0, 0, GPTIMER_COUNT_UP);
	timer_ll_enable_intr(&TIMERG0, TIMER_LL_EVENT_ALARM(0), false);
	timer_ll_clear_intr_status(&TIMERG0, TIMER_LL_EVENT_ALARM(0));
	timer_ll_enable_alarm(&TIMERG0, 0, true);
	timer_ll_enable_auto_reload(&TIMERG0, 0, true);
	timer_ll_enable_counter(&TIMERG0, 0, false);
	timer_ll_set_alarm_value(&TIMERG0, 0, 22);
	timer_ll_enable_intr(&TIMERG0, TIMER_LL_EVENT_ALARM(0), true);
	ESP_ERROR_CHECK(esp_intr_alloc_intrstatus(timer_group_periph_signals.groups[0].timer_irq_id[0], ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL5,
											  (uint32_t)timer_ll_get_intr_status_reg(&TIMERG0), TIMER_LL_EVENT_ALARM(0), NULL, NULL, NULL));
}

#endif // DRIVE_H__