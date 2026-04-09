#ifndef ANALOG_H__
#define ANALOG_H__

#include <stdint.h>

#define VREFINT_CAL_ADDR 0x1FFF7A2A
#define TS_CAL1_ADDR 0x1FFF7A2C
#define TS_CAL2_ADDR 0x1FFF7A2E

#define ADC_I_SCALE ((3.3f / 4095.0f) / (10.0f * 0.02f))
#define ADC_UB_SCALE ((3.3f / 4095.0f) * (1.0f + 39.0f / 3.3f))

enum
{
	ADC_CH_I2 = 0,	 // 1 - ch 8
	ADC_CH_I1,		 // 2 - ch 9
	ADC_CH_TEMP_INV, // 3 - ch 10

	ADC_CH_TEMP_CPU, // 1 - ch 16
	ADC_CH_UB,		 // 2 - ch 14
	ADC_CH_TEMP_MTR, // 3 - ch 11

	ADC_CH_INTREF, // 1 - ch 17
	ADC_CH_7,	   // 2 - none
	ADC_CH_8,	   // 3 - none

	ADC_CH_COUNT,
};

typedef struct
{
	uint16_t i_raw[2];
	float i[2];
	float ub;
	float tmtr;
	float tinv;
	float tcpu;
} adc_val_t;

void adc_init(void);
void adc_conv_val(void);

extern volatile uint16_t g_adc_buffer[ADC_CH_COUNT];
extern volatile adc_val_t g_adc_val;

#endif // ANALOG_H__