#include "stm32f4xx.h"

uint32_t SystemCoreClock = 168000000;
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void);
void NMI_Handler(void);

static void clock_reinit(void)
{
	RCC_DeInit();
	RCC_HSEConfig(RCC_HSE_ON);
	ErrorStatus errorStatus = RCC_WaitForHSEStartUp();
	RCC_LSICmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
		;

	if(errorStatus == SUCCESS)
	{
		RCC_PLLConfig(RCC_PLLSource_HSE, 4, 168, 2, 7);
		RCC->CR |= RCC_CR_CSSON;
		RCC_HSICmd(DISABLE);
	}
	else
	{
		RCC_PLLConfig(RCC_PLLSource_HSI, 8, 168, 2, 7);
		RCC->CR &= ~(RCC_CR_CSSON);
	}

	RCC_PLLCmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		;

	FLASH_SetLatency(FLASH_Latency_5);
	FLASH_PrefetchBufferCmd(ENABLE);
	FLASH->ACR |= FLASH_ACR_PRFTEN;
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK1Config(RCC_HCLK_Div4);
	RCC_PCLK2Config(RCC_HCLK_Div2);
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
}

void NMI_Handler(void)
{
	RCC->CIR |= RCC_CIR_CSSC;
	clock_reinit();
}

void SystemInit(void)
{
	SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2)); /* set CP10 and CP11 Full Access */

	clock_reinit();
}
