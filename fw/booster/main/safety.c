#include "safety.h"
#include "conf.h"
#include "dcc.h"
#include "ina226.h"
#include <string.h>

static volatile uint32_t errors = 0, errors_latched = 0;
float err_reg[16] = {0};
static uint32_t tmr_alert_dis = 0;

void safety_set_error(uint32_t bit, bool value, float reg)
{
	if(value)
	{
		if((errors_latched & (1 << bit)) == false) err_reg[bit] = reg;
		errors |= 1 << bit;
		errors_latched |= 1 << bit;
		dcc_enable(false);
	}
	else
	{
		errors &= (uint32_t)~(1 << bit);
	}
}

uint32_t safety_get_error_latched(void) { return errors_latched; }

void safety_reset_error(void)
{
	errors_latched &= errors;
	memset(err_reg, 0, sizeof(err_reg));
}

void safety_drive_enable(bool en)
{
	if(en && errors_latched == 0)
	{
		ina226_cur_limit_en(false);
		tmr_alert_dis = ON_TIME_LIM_DIS;
		dcc_enable(true);
	}
	if(en == false) dcc_enable(false);
}

void safety_poll(uint32_t diff_ms)
{
	if(tmr_alert_dis > diff_ms)
		tmr_alert_dis -= diff_ms;
	else
	{
		tmr_alert_dis = 0;
		ina226_cur_limit_en(true);
	}
}