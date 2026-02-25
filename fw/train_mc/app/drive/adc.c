#include "adc.h"
#include "stm32f4xx.h"

#define NTC_COEF 3434.0f
// NTC -> lo side
#define NTC_RES_LO_SIDE(adc_val) ((adc_val * 10000.0f) / (4095 - adc_val))
#define NTC_TEMP_LO_SIDE(adc_raw, coef) (1.0f / ((logf_fast(NTC_RES_LO_SIDE(adc_raw) / 10000.0f) / coef) + (1.0 / 298.15f)) - 273.15f)

static float adcVREFINTCAL, adcTSCAL1, adcTSSlopeK;

static inline int __float_as_int(float in)
{
	union fi
	{
		int i;
		float f;
	} conv;
	conv.f = in;
	return conv.i;
}

static inline float __int_as_float(int in)
{
	union fi
	{
		int i;
		float f;
	} conv;
	conv.i = in;
	return conv.f;
}

/* natural log on [0x1.f7a5ecp-127, 0x1.fffffep127]. Maximum relative error 9.4529e-5 */
static inline float logf_fast(float a)
{
	float m, r, s, t, i, f;
	int32_t e;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
	e = (__float_as_int(a) - 0x3f2aaaab) & 0xff800000;
#pragma GCC diagnostic pop

	m = __int_as_float(__float_as_int(a) - e);
	i = (float)e * 1.19209290e-7f; // 0x1.0p-23
	/* m in [2/3, 4/3] */
	f = m - 1.0f;
	s = f * f;
	/* Compute log1p(f) for f in [-1/3, 1/3] */
	r = (0.230836749f * f - 0.279208571f); // 0x1.d8c0f0p-3, -0x1.1de8dap-2
	t = (0.331826031f * f - 0.498910338f); // 0x1.53ca34p-2, -0x1.fee25ap-2
	r = (r * s + t);
	r = (r * s + f);
	r = (i * 0.693147182f + r); // 0x1.62e430p-1 // log(2)
	return r;
}

void adc_init(void)
{
	// @42MHz
	ADC_CommonInitTypeDef ADC_CommonInitStructure = {0};
	ADC_CommonInitStructure.ADC_Mode = ADC_TripleMode_RegSimult;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_1;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	ADC_InitTypeDef ADC_InitStructure = {0};
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Falling;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 3;

	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = 0;
	ADC_Init(ADC2, &ADC_InitStructure);
	ADC_Init(ADC3, &ADC_InitStructure);

	ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE);

	uint8_t rank = 1;
	ADC_RegularChannelConfig(ADC1, ADC_Channel_8, rank, ADC_SampleTime_15Cycles);
	ADC_RegularChannelConfig(ADC2, ADC_Channel_9, rank, ADC_SampleTime_15Cycles);
	ADC_RegularChannelConfig(ADC3, ADC_Channel_10, rank, ADC_SampleTime_15Cycles);
	rank++;
	ADC_RegularChannelConfig(ADC1, ADC_Channel_TempSensor, rank, ADC_SampleTime_112Cycles);
	ADC_RegularChannelConfig(ADC2, ADC_Channel_14, rank, ADC_SampleTime_112Cycles);
	ADC_RegularChannelConfig(ADC3, ADC_Channel_11, rank, ADC_SampleTime_112Cycles);
	rank++;
	ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, rank, ADC_SampleTime_112Cycles);
	ADC_RegularChannelConfig(ADC2, ADC_Channel_0, rank, ADC_SampleTime_112Cycles); // dummy
	ADC_RegularChannelConfig(ADC3, ADC_Channel_0, rank, ADC_SampleTime_112Cycles); // dummy

	adcVREFINTCAL = *(uint16_t *)VREFINT_CAL_ADDR;
	adcTSCAL1 = *(uint16_t *)TS_CAL1_ADDR;
	float adcTSCAL2 = *(uint16_t *)TS_CAL2_ADDR;
	adcTSSlopeK = (110.0f - 30.0f) / (adcTSCAL2 - adcTSCAL1);

	ADC_Cmd(ADC1, ENABLE);
	ADC_Cmd(ADC2, ENABLE);
	ADC_Cmd(ADC3, ENABLE);
	ADC_TempSensorVrefintCmd(ENABLE);
}

void adc_conv_val(void)
{
	g_adc_val.i_raw[0] = g_adc_buffer[ADC_CH_I1];
	g_adc_val.i_raw[1] = g_adc_buffer[ADC_CH_I2];

	g_adc_val.i[1] = (float)g_adc_buffer[ADC_CH_I1] * ADC_I_SCALE - 2048;
	g_adc_val.i[2] = (float)g_adc_buffer[ADC_CH_I2] * ADC_I_SCALE - 2048;
	g_adc_val.ub = (float)g_adc_buffer[ADC_CH_UB] * ADC_UB_SCALE;

	g_adc_val.tmtr = NTC_TEMP_LO_SIDE(g_adc_buffer[ADC_CH_TEMP_MTR], NTC_COEF);
	g_adc_val.tinv = NTC_TEMP_LO_SIDE(g_adc_buffer[ADC_CH_TEMP_INV], NTC_COEF);
	g_adc_val.tcpu = ((float)g_adc_buffer[ADC_CH_TEMP_CPU] * adcVREFINTCAL / (float)g_adc_buffer[ADC_CH_INTREF] - adcTSCAL1) * adcTSSlopeK + 30.0f;
}