#include "dcc.h"
#include <stdlib.h>

void dcc_cd_add_pkt(const uint8_t *data, uint32_t sz)
{
	uint8_t checksum = 0;
	for(uint32_t i = 0; i < sz; i++)
	{
		checksum ^= data[i];
	}

	for(uint8_t i = 0; i < 12; i++) // preamble
		dcc_cd_add_bit(true);

	dcc_cd_add_bit(false); // start bit

	for(uint32_t i = 0; i < sz; i++)
	{
		for(uint8_t j = 0; j < 8; j++)
			dcc_cd_add_bit(data[i] & (1 << (7 - j)));

		dcc_cd_add_bit(false); // start bit
	}

	for(uint8_t j = 0; j < 8; j++)
		dcc_cd_add_bit(checksum & (1 << (7 - j)));

	dcc_cd_add_bit(true); // end bit

	dcc_cd_add_cutout();
}

void dcc_cd_cmd_custom(uint8_t addr, uint8_t cmd, const uint8_t *data, uint32_t sz)
{
	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b11000000 | (cmd & 0x1F);
	for(uint32_t j = 0; j < sz; j++)
	{
		buf[l++] = data[j];
	}

	dcc_cd_add_pkt(buf, l);
}

void dcc_cd_cmd_advanced(uint8_t addr, uint8_t cmd, uint8_t val)
{
	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b00100000 | (cmd & 0x1F);
	buf[l++] = val;

	dcc_cd_add_pkt(buf, l);
}

void dcc_cd_cmd_speed_128(uint8_t addr, int8_t speed, bool emcy_stop)
{
	speed = speed == 1 ? 2 : speed;
	speed = ((speed >= 0) ? 0x80 : 0x00) | (((speed < 0) ? -speed : speed) & 0x7F);

	if(emcy_stop) speed = 1;
	dcc_cd_cmd_advanced(addr, 0b00011111, speed);
}

void dcc_cd_cmd_speed_28(uint8_t addr, int8_t speed, bool emcy_stop)
{
	uint8_t inv_code = speed < 0 ? 0b00100000 : 0;

	speed = speed == 1 ? 2 : speed;
	if(emcy_stop) speed = 1;

	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b01000000 | inv_code | (abs(speed) & 0x1F);

	dcc_cd_add_pkt(buf, l);
}

void dcc_cd_cmd_analog_func(uint8_t addr, uint8_t out, uint8_t value)
{
	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b00111101;
	buf[l++] = out;
	buf[l++] = value;

	dcc_cd_add_pkt(buf, l);
}

void dcc_cd_cmd_func_grp1(uint8_t addr, uint8_t value)
{
	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b10000000 | (value & 0x1F);

	dcc_cd_add_pkt(buf, l);
}

void dcc_cd_cmd_func_grp2(uint8_t addr, uint8_t value)
{
	uint8_t buf[DCC_MAX_BYTES] = {0};
	uint32_t l = 0;
	buf[l++] = addr;
	buf[l++] = 0b10100000 | (value & 0x1F);

	dcc_cd_add_pkt(buf, l);
}