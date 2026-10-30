#include "../dcc.h"
#include <stdio.h>

int cnt_cb = 0;

void dcc_dc_cb_spd_128(uint8_t code)
{
	cnt_cb++;
	printf(" [===%s===%d]", __func__, code);
}

void dcc_dc_cb_an_func(uint8_t addr, uint8_t value)
{
	cnt_cb++;
	printf(" [===%s===%d %d]", __func__, addr, value);
}

void dcc_dc_cb_spd28_rev(uint8_t value)
{
	cnt_cb++;
	printf(" [===%s===%d]", __func__, value);
}

void dcc_dc_cb_spd28_fwd(uint8_t value)
{
	cnt_cb++;
	printf(" [===%s===%d]", __func__, value);
}

uint8_t g_dcc_self_id = 1;

#include "../dcc_coder.c"
#include "../dcc_decoder.c"

static uint32_t c = 0;

void dcc_cd_add_bit(bool is_1)
{
	int8_t v = is_1 ? 2 : 1;
	printf("%c", is_1 ? '1' : '0');
	dcc_dc_push(is_1 ? DCC_BIT_1 : DCC_BIT_0);
	dcc_dc_poll();
	dcc_dc_push(-1 * (is_1 ? DCC_BIT_1 : DCC_BIT_0));
	dcc_dc_poll();
	c++;
	if(c == 12) printf(" ");
	if(c >= 13)
	{
		if(((c - 12) % 9) == 0) printf(" ");
		if(((c - 12) % 9) == 1) printf(" ");
	}
}

void dcc_cd_add_cutout(void) {}

int main(void)
{
	dcc_cd_cmd_speed_128(1, 27, false);
	printf("\n");
	c = 0;

	dcc_cd_cmd_speed_28(1, 7, false);
	printf("\n");
	c = 0;

	dcc_cd_cmd_speed_28(1, -8, false);
	printf("\n");
	c = 0;

	dcc_cd_cmd_analog_func(1, 0xAA, 0x55);
	printf("\n");
	c = 0;

	// dcc_cd_cmd_func_grp1(1, 0xBB);
	// printf("\n");
	// c = 0;

	// dcc_cd_cmd_func_grp2(1, 0xCC);
	// printf("\n");
	// c = 0;
	return cnt_cb == 0;
}