#include "i2c_common.h"
#include "stm32f4xx.h"

#define TIMEOUT 0xFFF
#define TIMEOUT_SHORT 100

#define I2C_FLAG_MASK 0x0000FFFFU

#define __HAL_I2C_CLEAR_FLAG(__FLAG__) (I2C1->SR1 = ~((__FLAG__) & I2C_FLAG_MASK))

#define __HAL_I2C_GET_FLAG(__FLAG__) ((((uint8_t)((__FLAG__) >> 16U)) == 0x01U)                                                            \
										  ? ((((I2C1->SR1) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET) \
										  : ((((I2C1->SR2) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET))

#define I2C_7BIT_ADD_WRITE(__ADDRESS__) ((uint8_t)((__ADDRESS__) & (uint8_t)(~I2C_OAR1_ADD0)))
#define I2C_7BIT_ADD_READ(__ADDRESS__) ((uint8_t)((__ADDRESS__) | I2C_OAR1_ADD0))
#define UNUSED(X) (void)X

#define I2C_MEM_ADD_MSB(__ADDRESS__) ((uint8_t)((uint16_t)(((uint16_t)((__ADDRESS__) & (uint16_t)0xFF00)) >> 8)))
#define I2C_MEM_ADD_LSB(__ADDRESS__) ((uint8_t)((uint16_t)((__ADDRESS__) & (uint16_t)0x00FF)))

#define __HAL_I2C_CLEAR_ADDRFLAG()    \
	do                                \
	{                                 \
		__IO uint32_t tmpreg = 0x00U; \
		tmpreg = I2C1->SR1;           \
		tmpreg = I2C1->SR2;           \
		UNUSED(tmpreg);               \
	} while(0)

//============================

#define _I2C_SR1_SB 0
#define _I2C_SR1_ADDR 1
#define _I2C_SR1_BTF 2
#define _I2C_SR1_RxNE 6
#define _I2C_SR1_TxE 7

#define _I2C_SR2_MSL 0

#define _I2C_CR1_START 8
#define _I2C_CR1_STOP 9
#define _I2C_CR1_ACK 10

#define _I2C_SB_FLAG (1 << _I2C_SR1_SB)
#define _I2C_ADDR_FLAG (1 << _I2C_SR1_ADDR)
#define _I2C_BTF_FLAG (1 << _I2C_SR1_BTF)
#define _I2C_RxNE_FLAG (1 << _I2C_SR1_RxNE)
#define _I2C_TxE_FLAG (1 << _I2C_SR1_TxE)

#define HAL_I2C_FLAG_OVR 0x00010800U
#define HAL_I2C_FLAG_AF 0x00010400U
#define HAL_I2C_FLAG_ARLO 0x00010200U
#define HAL_I2C_FLAG_BERR 0x00010100U
#define HAL_I2C_FLAG_TXE 0x00010080U
#define HAL_I2C_FLAG_RXNE 0x00010040U
#define HAL_I2C_FLAG_STOPF 0x00010010U
#define HAL_I2C_FLAG_ADD10 0x00010008U
#define HAL_I2C_FLAG_BTF 0x00010004U
#define HAL_I2C_FLAG_ADDR 0x00010002U
#define HAL_I2C_FLAG_SB 0x00010001U
#define HAL_I2C_FLAG_DUALF 0x00100080U
#define HAL_I2C_FLAG_GENCALL 0x00100010U
#define HAL_I2C_FLAG_TRA 0x00100004U
#define HAL_I2C_FLAG_BUSY 0x00100002U
#define HAL_I2C_FLAG_MSL 0x00100001U

#define HAL_I2C_ERROR_NONE 0x00000000U	  /*!< No error              */
#define HAL_I2C_ERROR_AF 0x00000004U	  /*!< AF error              */
#define HAL_I2C_ERROR_TIMEOUT 0x00000020U /*!< Timeout Error         */
#define HAL_I2C_WRONG_START 0x00000200U	  /*!< Wrong start Error     */

__IO uint32_t ErrorCode = 0;

uint32_t i2c_err_restarts = 0;

static uint32_t time = 0;

void i2c_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStruct;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_I2C1);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	I2C_DeInit(I2C1);

	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStruct.I2C_OwnAddress1 = 0xFE;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStruct.I2C_ClockSpeed = 400000;
	I2C_Init(I2C1, &I2C_InitStruct);

	I2C_Cmd(I2C1, ENABLE);
}

static int I2C_WaitOnFlagUntilTimeout(uint32_t Flag, bool Status)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(Flag) == Status)
	{
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(Flag) == Status))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int check_busy_(void)
{
	// eset the peripheral.
	// Toggle SCL 9 times at an appropriate clock speed while holding SDA high to clear slaves.
	// Re-initialize the peripheral an associated software state machine.

	if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BUSY))
	{
		if(I2C1->SR1 & (0xFFFF & (I2C_FLAG_OVR | I2C_FLAG_AF | I2C_FLAG_ARLO | I2C_FLAG_BERR)))
		{
			i2c_err_restarts++;
			I2C_Cmd(I2C1, DISABLE);
			GPIO_InitTypeDef GPIO_InitStructure;
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
			GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
			GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
			GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
			GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
			GPIO_Init(GPIOB, &GPIO_InitStructure);
			for(uint32_t i = 0; i < 9; i++)
			{
				GPIOB->BSRRH = 1 << 6;
				for(volatile uint32_t j = 0; j < 128; j++)
					;
				GPIOB->BSRRL = 1 << 6;
				for(volatile uint32_t j = 0; j < 128; j++)
					;
			}
			i2c_init();
		}
		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BUSY)) return I2C_ERR_BUSY;
		return 0;
	}
	return 0;
}

static int check_busy(void)
{
	time = TIMEOUT;
	while(check_busy_() != 0)
	{
		if(--time == 0) return -1;
	}
	return 0;
}

static int I2C_IsAcknowledgeFailed(void)
{
	if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF) == SET)
	{
		__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF); /* Clear NACKF Flag */
		ErrorCode |= HAL_I2C_ERROR_AF;
		return -1;
	}
	return 0;
}

static int I2C_WaitOnTXEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_TXE) == RESET)
	{
		if(I2C_IsAcknowledgeFailed()) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_TXE) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_WaitOnMasterAddressFlagUntilTimeout(uint32_t Flag)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(Flag) == RESET)
	{
		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF);
			ErrorCode |= HAL_I2C_ERROR_AF;
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(Flag) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_WaitOnRXNEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_RXNE) == RESET)
	{
		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_STOPF) == SET)
		{
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_STOPF);
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_RXNE) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_RequestMemoryWrite(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit)
{
	SET_BIT(I2C1->CR1, I2C_CR1_START);
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START)
		{
			ErrorCode = HAL_I2C_WRONG_START;
		}
		return -3;
	}
	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	if(is_mem_16_bit == false)
	{
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}
	else // 16 bit
	{
		I2C1->DR = I2C_MEM_ADD_MSB(MemAddress);
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return -1;
		}
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}

	return 0;
}

static int I2C_RequestMemoryRead(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit)
{
	SET_BIT(I2C1->CR1, I2C_CR1_ACK);
	SET_BIT(I2C1->CR1, I2C_CR1_START);

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -3;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	if(is_mem_16_bit == false)
	{
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}
	else
	{
		I2C1->DR = I2C_MEM_ADD_MSB(MemAddress);
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return -1;
		}

		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}

	if(I2C_WaitOnTXEFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
		return -1;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_START); // Generate Restart

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -3;
	}

	I2C1->DR = I2C_7BIT_ADD_READ(addr);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;
	return 0;
}

static int I2C_MasterRequestWrite(uint16_t DevAddress)
{
	SET_BIT(I2C1->CR1, I2C_CR1_START); /* Generate Start */

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET)) /* Wait until SB flag is set */
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}
	I2C1->DR = I2C_7BIT_ADD_WRITE(DevAddress);								  /* Send slave address */
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1; /* Wait until ADDR flag is set */
	return 0;
}

static int I2C_MasterRequestRead(uint16_t DevAddress)
{
	SET_BIT(I2C1->CR1, I2C_CR1_ACK);   /* Enable Acknowledge */
	SET_BIT(I2C1->CR1, I2C_CR1_START); /* Generate Start */

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET)) /* Wait until SB flag is set */
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_READ(DevAddress);								  /* Send slave address */
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1; /* Wait until ADDR flag is set */
	return 0;
}

static int I2C_WaitOnBTFFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET)
	{
		if(I2C_IsAcknowledgeFailed()) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////

int i2c_mem_write(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, void *pData, uint16_t Size)
{
	if(check_busy()) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_RequestMemoryWrite(addr, MemAddress, is_mem_16_bit)) return I2C_ERR_GENERAL;

	while(XferSize > 0)
	{
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return I2C_ERR_GENERAL;
		}

		I2C1->DR = *pBuffPtr;
		pBuffPtr++;
		XferSize--;
		XferCount--;

		if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET) && (XferSize))
		{
			I2C1->DR = *pBuffPtr;
			pBuffPtr++;
			XferSize--;
			XferCount--;
		}
	}

	if(I2C_WaitOnBTFFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return I2C_ERR_GENERAL;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_STOP);

	return 0;
}

int i2c_mem_read(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, void *pData, uint16_t Size)
{
	if(check_busy()) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_RequestMemoryRead(addr, MemAddress, is_mem_16_bit)) return I2C_ERR_GENERAL;

	if(XferSize == 0)
	{
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP);
	}
	else if(XferSize == 1U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP);
	}
	else if(XferSize == 2U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
		SET_BIT(I2C1->CR1, I2C_CR1_POS);
		__HAL_I2C_CLEAR_ADDRFLAG();
	}
	else
	{
		__HAL_I2C_CLEAR_ADDRFLAG();
	}

	while(XferSize > 0)
	{
		if(XferSize <= 3U)
		{
			if(XferSize == 1U)
			{
				if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL;
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else if(XferSize == 2U)
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL;
				SET_BIT(I2C1->CR1, I2C_CR1_STOP);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else /* 3 Last bytes */
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL;
				CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL;
				SET_BIT(I2C1->CR1, I2C_CR1_STOP);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
		else
		{
			if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL;
			*pBuffPtr = (uint8_t)I2C1->DR;
			pBuffPtr++;
			XferSize--;
			XferCount--;
			if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET)
			{
				if(XferSize == 3U) CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
	}
	return 0;
}

int i2c_is_device_ready(uint16_t addr, uint32_t Trials)
{
	if(check_busy()) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;

	uint32_t try = 0;
	do
	{
		time = TIMEOUT_SHORT;

		SET_BIT(I2C1->CR1, I2C_CR1_START);
		if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
		{
			if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
			return I2C_ERR_GENERAL;
		}

		I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

		bool tmp1 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR);
		bool tmp2 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF);
		while(time && (tmp1 == RESET) && (tmp2 == RESET))
		{
			--time;
			tmp1 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR);
			tmp2 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF);
		}

		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_ADDRFLAG();
			if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_GENERAL;
			return 0;
		}
		else
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF);
			if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_GENERAL;
		}
		try++;
	} while(try < Trials);

	return I2C_ERR_TIMEOUT;
}

int i2c_tx(uint16_t DevAddress, void *pData, uint16_t Size)
{
	if(check_busy()) return I2C_ERR_BUSY; /* Wait until BUSY flag is reset */

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS); /* Disable Pos */
	ErrorCode = HAL_I2C_ERROR_NONE;
	const uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_MasterRequestWrite(DevAddress)) return I2C_ERR_GENERAL; /* Send Slave Address */
	__HAL_I2C_CLEAR_ADDRFLAG();

	while(XferSize > 0U)
	{
		if(I2C_WaitOnTXEFlagUntilTimeout()) /* Wait until TXE flag is set */
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
			return I2C_ERR_GENERAL;
		}

		I2C1->DR = *pBuffPtr;
		pBuffPtr++;
		XferCount--;
		XferSize--;

		if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET) && (XferSize != 0U))
		{
			I2C1->DR = *pBuffPtr;
			pBuffPtr++;
			XferCount--;
			XferSize--;
		}

		if(I2C_WaitOnBTFFlagUntilTimeout()) /* Wait until BTF flag is set */
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
			return I2C_ERR_GENERAL;
		}
	}

	SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
	return 0;
}

int i2c_rx(uint16_t DevAddress, void *pData, uint16_t Size)
{
	if(check_busy()) return I2C_ERR_BUSY; /* Wait until BUSY flag is reset */

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS); /* Disable Pos */
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_MasterRequestRead(DevAddress)) return I2C_ERR_GENERAL; /* Send Slave Address */

	if(XferSize == 0U)
	{
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
	}
	else if(XferSize == 1U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
	}
	else if(XferSize == 2U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
		SET_BIT(I2C1->CR1, I2C_CR1_POS);   /* Enable Pos */
		__HAL_I2C_CLEAR_ADDRFLAG();
	}
	else
	{
		SET_BIT(I2C1->CR1, I2C_CR1_ACK); /* Enable Acknowledge */
		__HAL_I2C_CLEAR_ADDRFLAG();
	}

	while(XferSize > 0U)
	{
		if(XferSize <= 3U)
		{
			if(XferSize == 1U)
			{
				if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL; /* Wait until RXNE flag is set */

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else if(XferSize == 2U)
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL; /* Wait until BTF flag is set */

				SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else /* 3 Last bytes */
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL; /* Wait until BTF flag is set */
				CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);												/* Disable Acknowledge */

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;

				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL; /* Wait until BTF flag is set */

				SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
		else
		{
			if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL; /* Wait until RXNE flag is set */

			*pBuffPtr = (uint8_t)I2C1->DR;
			pBuffPtr++;
			XferSize--;
			XferCount--;

			if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET)
			{
				if(XferSize == 3U) CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
	}

	return 0;
}