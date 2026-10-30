#include "dcc_hw.h"
#include "dcc.h"
#include "dcc_fb.h"

#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_intr_alloc.h"
#include "hal/gpio_ll.h"
#include "hal/timer_ll.h"
#include "soc/periph_defs.h"
#include "soc/timer_periph.h"

#include "console.h"

#define DCC_BUF_CNT 1024

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

extern void periph_module_enable(periph_module_t periph);

static inline void dcc_tmr_config(void)
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

volatile uint8_t dcc_on = false;

volatile uint16_t dccq[2 * DCC_BUF_CNT] = {0}; // format: [ length{us}, type: 0=>+, 1=>-, other=>short ]

volatile uint32_t dcc_cnt = 512;
volatile uint32_t dcc_tail = 0;
volatile uint32_t dcc_head = 0;
volatile uint8_t dcc_1_continue = 0;

// 450us 100us 58us

void dcc_hw_init(void)
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

	dcc_tmr_config();

	memset((void *)dccq, 0, sizeof(dccq));
	timer_ll_enable_counter(&TIMERG0, 0, true);

	dcc_dc_cb_init();
}

bool dcc_is_enabled(void) { return dcc_on; }

void dcc_enable(bool en)
{
	bool is_off = dcc_on && en == false;
	dcc_on = en;
	if(is_off)
	{
		PIN_WR(PIN_EN0, 0);
		PIN_WR(PIN_EN1, 0);
	}
}

static void dcc_cd_add_frm(uint16_t len, uint16_t type)
{
	volatile uint32_t head_new = dcc_head;
	head_new = (head_new + 1) % dcc_cnt;
	dccq[head_new * 2 + 0] = len;
	dccq[head_new * 2 + 1] = type;
	dcc_head = head_new;
}

void dcc_cd_add_bit(bool is_1)
{
	if(is_1)
	{
		// dcc_cd_add_frm(200, DCC_POLAR_CUTOUT);
		dcc_cd_add_frm(DCC_LEN_1, DCC_POLAR_P);
		dcc_cd_add_frm(DCC_LEN_1, DCC_POLAR_N);
	}
	else
	{
		// dcc_cd_add_frm(200, DCC_POLAR_CUTOUT);
		dcc_cd_add_frm(DCC_LEN_0, DCC_POLAR_P);
		dcc_cd_add_frm(DCC_LEN_0, DCC_POLAR_N);
	}
}

void dcc_cd_add_cutout(void)
{
	dcc_cd_add_frm(200, DCC_POLAR_CUTOUT);
}

void dcc_manual(bool on0, bool on1, bool state0, bool state1)
{
	PIN_WR(PIN_EN0, on0);
	PIN_WR(PIN_EN1, on1);
	PIN_WR(PIN_LVL0, state0);
	PIN_WR(PIN_LVL1, state1);
}

void drv_loop(void) // for ASM IRQ ref.
{
	REG_SET_BIT(TIMG_T0CONFIG_REG(0), TIMG_T0_ALARM_EN_M);
	REG_WRITE(TIMG_INT_CLR_TIMERS_REG(0), TIMG_T0_INT_CLR_M);

	if(dcc_on == false)
	{
		PIN_WR(PIN_EN0, 0);
		PIN_WR(PIN_EN1, 0);
		PIN_WR(PIN_LVL0, 0);
		PIN_WR(PIN_LVL1, 0);
		REG_WRITE(TIMG_T0ALARMLO_REG(0), DCC_LEN_1);
	}
	else
	{
		if(dcc_tail == dcc_head || dcc_1_continue) // feed DCC_LEN_1
		{
			dcc_1_continue = dcc_1_continue ? 0 : 1;
			PIN_WR(PIN_EN0, 1);
			PIN_WR(PIN_EN1, 1);
			if(dcc_1_continue)
			{
				PIN_WR(PIN_LVL0, 0);
				PIN_WR(PIN_LVL1, 1);
			}
			else
			{
				PIN_WR(PIN_LVL1, 0);
				PIN_WR(PIN_LVL0, 1);
			}
			REG_WRITE(TIMG_T0ALARMLO_REG(0), DCC_LEN_1);
		}
		else
		{
			if(++dcc_tail >= dcc_cnt) dcc_tail = 0;
			volatile uint16_t *p = &dccq[dcc_tail * 2];

			PIN_WR(PIN_EN0, 1);
			PIN_WR(PIN_EN1, 1);
			if(p[1] == 0)
			{
				PIN_WR(PIN_LVL0, 0);
				PIN_WR(PIN_LVL1, 1);
			}
			else if(p[1] == 1)
			{
				PIN_WR(PIN_LVL1, 0);
				PIN_WR(PIN_LVL0, 1);
			}
			else
			{
				PIN_WR(PIN_LVL0, 0);
				PIN_WR(PIN_LVL1, 0);
			}
			REG_WRITE(TIMG_T0ALARMLO_REG(0), *p);
		}
	}
}

void dcc_diag(print_func_t pf)
{
	pf("%s H T Cnt: %d %d %d\n", dcc_on ? "ON" : "OFF", dcc_head, dcc_tail, dcc_cnt);
}