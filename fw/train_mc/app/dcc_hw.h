#ifndef DCC_HW_H__
#define DCC_HW_H__

#include <stdint.h>

void u2_tx(const uint8_t *data, uint16_t length);

void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void TIM3_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);

#endif // DCC_HW_H__