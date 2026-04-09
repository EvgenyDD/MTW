#include "adc.h"
#include "stm32f4xx.h"

#define SAMPLE_ACC_COUNT 128

#define VREFINT_CAL_ADDR 0x1FFF7A2A
#define TS_CAL1_ADDR 0x1FFF7A2C
#define TS_CAL2_ADDR 0x1FFF7A2E

enum
{
	ADC_CH_I_CHRG = 0,
	ADC_CH_V_BAT,
	ADC_CH_T_MCU,
	ADC_CH_VREFINT,
	ADC_CH,
};

adc_val_t adc_val = {0, 4.1f, 25.0f};

static volatile uint16_t adc_buf[ADC_CH];
static volatile uint32_t adc_buf_acc[ADC_CH][SAMPLE_ACC_COUNT] = {0};
static volatile uint32_t adc_buf_ptr = 0;

static float adcVREFINTCAL, adcTSCAL1, adcTSSlopeK;

void adc_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	DMA_DeInit(DMA2_Stream0);
	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)adc_buf;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = ADC_CH;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream0, &DMA_InitStructure);
	DMA_Cmd(DMA2_Stream0, ENABLE);

	ADC_DeInit();

	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = ADC_CH;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_480Cycles); // PA1
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 2, ADC_SampleTime_480Cycles); // PA6
	ADC_RegularChannelConfig(ADC1, ADC_Channel_TempSensor, 3, ADC_SampleTime_480Cycles);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 4, ADC_SampleTime_480Cycles);

	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

	ADC_DMACmd(ADC1, ENABLE);

	ADC_Cmd(ADC1, ENABLE);
	ADC_ContinuousModeCmd(ADC1, ENABLE);

	ADC_TempSensorVrefintCmd(ENABLE);

	ADC_SoftwareStartConv(ADC1);

	adcVREFINTCAL = *(uint16_t *)VREFINT_CAL_ADDR;
	adcTSCAL1 = *(uint16_t *)TS_CAL1_ADDR;
	float adcTSCAL2 = *(uint16_t *)TS_CAL2_ADDR;
	adcTSSlopeK = (110.0f - 30.0f) / (adcTSCAL2 - adcTSCAL1);
}

void adc_track(void)
{
	volatile uint32_t buf_acc[ADC_CH] = {0};

	for(uint32_t i = 0; i < ADC_CH; i++)
	{
		adc_buf_acc[i][adc_buf_ptr] = adc_buf[i];
		for(uint32_t j = 0; j < SAMPLE_ACC_COUNT; j++)
		{
			buf_acc[i] += adc_buf_acc[i][j];
		}
		buf_acc[i] /= SAMPLE_ACC_COUNT;
	}
	if(++adc_buf_ptr >= SAMPLE_ACC_COUNT) adc_buf_ptr = 0;

	adc_val.i_chrg = (float)buf_acc[ADC_CH_I_CHRG];
	adc_val.v_bat = (float)buf_acc[ADC_CH_V_BAT] / 4095.0f * 3.3f * (1.0f + 1000.0f / 470.0f);
	adc_val.t_mcu = ((float)buf_acc[ADC_CH_T_MCU] * adcVREFINTCAL / (float)buf_acc[ADC_CH_VREFINT] - adcTSCAL1) * adcTSSlopeK + 30.0f;
}
