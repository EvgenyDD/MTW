#include "dcc.h"
#include <stdlib.h>

#ifdef DBG_STM
#include "platform.h"
#endif

enum
{
	PH_WAIT_PREAMBLE = 0,
	PH_WAIT_SEPARATOR,
	PH_WAIT_SEPARATOR_IN_DATA,
	PH_WAIT_DATA
};

static int prev_polarity = -1, prev_type = -1;

static struct
{
	uint8_t ph;
	uint32_t cnt_1;

	uint8_t data_byte;
	uint32_t cnt_data_byte_bits;

	uint8_t payload[DCC_MAX_BYTES];
	uint32_t payload_cnt;
} p = {0};

#define PULSE_Q_LEN 256

static struct
{
	volatile int8_t pulse[PULSE_Q_LEN];
	volatile uint32_t head, tail;
} q = {0};

static void parse_pkt(const uint8_t *data, uint32_t len);

static void discard_pkt(void)
{
#ifdef DBG_STM
	GPIOB->ODR |= (1 << 2);
	GPIOC->ODR |= (1 << 7);
	GPIOB->ODR |= (1 << 12);
#endif

	p.cnt_1 = 0;
	p.ph = PH_WAIT_PREAMBLE;
}

static void check_pkt(uint8_t bit)
{
	switch(p.ph)
	{
	case PH_WAIT_PREAMBLE:
		if(bit == DCC_BIT_1)
		{
			p.cnt_1++;
		}
		else
		{
			discard_pkt();
		}
		if(p.cnt_1 >= PREAMBLE_LEN_MIN)
		{
			PRINTF("@");
			p.payload_cnt = 0;
			p.cnt_data_byte_bits = 0;
			p.ph = PH_WAIT_SEPARATOR;
		}
		break;

	case PH_WAIT_SEPARATOR:
		if(bit == DCC_BIT_0)
		{
			PRINTF("#");
			p.ph = PH_WAIT_DATA;
			p.cnt_data_byte_bits = 0;
			p.data_byte = 0;
		}
		break;

	case PH_WAIT_SEPARATOR_IN_DATA:
		if(bit == DCC_BIT_0)
		{
			PRINTF("!");
			p.ph = PH_WAIT_DATA;
			p.cnt_data_byte_bits = 0;
			p.data_byte = 0;
		}
		else
		{
			if(p.payload_cnt > 2)
			{
				uint8_t cs = 0;
				for(uint32_t i = 0; i < p.payload_cnt - 1; i++)
				{
					cs ^= p.payload[i];
				}

				if(cs == p.payload[p.payload_cnt - 1])
				{
					parse_pkt(p.payload, p.payload_cnt - 1);
				}
				else
				{
					PRINTF("{WRONG CRC! %x %x}", cs, p.payload[p.payload_cnt - 1]);
				}
			}
			PRINTF("~");
			discard_pkt();
		}
		break;

	case PH_WAIT_DATA:
		p.data_byte |= (bit == DCC_BIT_1) << (7 - p.cnt_data_byte_bits);
		if(++p.cnt_data_byte_bits >= 8)
		{
			PRINTF("^");
			p.cnt_data_byte_bits = 0;
			p.ph = PH_WAIT_SEPARATOR_IN_DATA;
			p.payload[p.payload_cnt++] = p.data_byte;
			if(p.payload_cnt >= DCC_MAX_BYTES) // discard long packet
			{
				PRINTF("~");
				discard_pkt();
			}
		}
		break;

	default:
		discard_pkt();
		break;
	}
}

static void parse_pkt(const uint8_t *data, uint32_t len)
{
	PRINTF(" [PKT %d: ", len);
	for(uint32_t i = 0; i < len; i++)
	{
		PRINTF("x%x ", p.payload[i]);
	}
	PRINTF("]");

	if(len == 0 || len > DCC_MAX_BYTES) return;

	if(len < 2) return;
	if(data[0] == DCC_ID_BROADCAST ||
	   data[0] == g_dcc_self_id)
	{
		uint8_t CCC = data[1] & 0xE0;
		PRINTF("pkt[x%x]", CCC);
		switch(CCC)
		{
		case 0x0: // Decoder and Consist Control Instruction
			break;

		case 0x20: // Advanced Operations Instruction
		{
			uint8_t GGGGG = data[1] & 0x1F;
			switch(GGGGG)
			{
			case 0x1F: // 128 Speed Step Control
				if(len == 3)
				{
					dcc_dc_cb_spd_128(data[2]);
				}
				break;

			case 0x1D: // Analog Function Group
				if(len == 4)
				{
					dcc_dc_cb_an_func(data[2], data[3]);
				}

				break;

			default: break;
			}
			break;
		}

		case 0x40: // Reverse
			if(len == 2)
			{
				dcc_dc_cb_spd28_rev(data[1] & 0x1F);
			}
			break;

		case 0x60: // Forward
			if(len == 2)
			{
				dcc_dc_cb_spd28_fwd(data[1] & 0x1F);
			}
			break;

		default: break;
		}
	}
}

#ifdef DBG_STM

#define D()                                   \
	for(volatile uint32_t i = 0; i < 16; i++) \
		;

static void t(int8_t abs_bit)
{
	if(abs_bit == 1)
	{
		GPIOB->ODR |= (1 << 2);
		asm("nop");
		asm("nop");
		asm("nop");
		GPIOB->ODR &= ~(1 << 2);
	}
	else if(abs_bit == 0)
	{
		GPIOC->ODR |= (1 << 7);
		asm("nop");
		asm("nop");
		asm("nop");
		GPIOC->ODR &= ~(1 << 7);
	}
	else
	{
		GPIOB->ODR |= (1 << 2);
		GPIOC->ODR |= (1 << 7);
		asm("nop");
		asm("nop");
		asm("nop");
		GPIOB->ODR &= ~(1 << 2);
		GPIOC->ODR &= ~(1 << 7);
	}
}
#endif

void dcc_dc_push(int8_t pulse)
{
	volatile uint32_t head_next = (q.head + 1) % PULSE_Q_LEN;
	if(head_next != q.tail)
	{
		q.pulse[q.head] = pulse;
		q.head = head_next;
	}
	else
	{
		// err_ovf++;
	}
}

void dcc_dc_poll(void)
{
	while(q.head != q.tail)
	{
		volatile uint32_t next_tail = (q.tail + 1) % PULSE_Q_LEN;
		volatile int8_t bit = q.pulse[q.tail];
		q.tail = next_tail;

		{
#ifdef DBG_STM
			GPIOB->ODR &= ~(1 << 2);
			GPIOB->ODR &= ~(1 << 12);
			GPIOC->ODR &= ~(1 << 7);
#endif
			int abs_bit = bit < 0 ? -bit : bit;
			int polarity = bit > 0;
			// printf("{%d %d}", abs_bit, polarity);

			if(prev_type == abs_bit && prev_polarity != polarity && prev_polarity != -1)
			{
#ifdef DBG_STM
				t(abs_bit);
#endif
				if(abs_bit != DCC_BIT_0 && abs_bit != DCC_BIT_1)
				{
#ifdef DBG_STM
					uint8_t v = (abs_bit << 4) | p.ph;
					// u2_tx(&v, 1);
#endif
					discard_pkt();
				}
				else
				{
#ifdef DBG_STM
					uint8_t v = (abs_bit << 4) | p.ph;
					// u2_tx(&v, 1);
#endif
					check_pkt(abs_bit);
#ifdef DBG_STM
					// u2_tx(&p.ph, 1);
#endif
				}
				prev_polarity = prev_type = -1;
			}
			else
			{
#ifdef DBG_STM
				GPIOB->ODR |= (1 << 12);
				asm("nop");
				asm("nop");
				asm("nop");
				GPIOB->ODR &= ~(1 << 12);
#endif
				prev_type = abs_bit;
				prev_polarity = polarity;
			}
		}
	}
}