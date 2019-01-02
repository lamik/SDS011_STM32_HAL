/*
 * sds011.c
 *
 *	The MIT License.
 *  Created on: 12.12.2018
 *		Author: Mateusz Salamon
 *		www.msalamon.pl
 *		mateusz@msalamon.pl
 *
 *	https://msalamon.pl/pomiar-czystosci-powietrza-przy-pomocy-sds011/
 *	https://github.com/lamik/SDS011_STM32_HAL
 */

#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "tim.h"

#include "sds011.h"

#ifdef SDS011_UART
UART_HandleTypeDef *huart_sds;
uint8_t Sds011_Buffer[SDS011_BUFFER_SIZE];
uint8_t Sds011_CommandBuffer[19];
uint8_t Sds011_Received;
#endif

#ifdef SDS011_PWM
TIM_HandleTypeDef *htim_sds;
uint32_t Pm2_5_TimChannel_sds;
uint32_t Pm10_TimChannel_sds;
#endif

volatile uint16_t Pm2_5;
volatile uint16_t Pm10;

#ifdef SDS011_UART
static const uint8_t Sds011_SleepCommand[] = {
		0xAA,
		0xB4,
		0x06,
		0x01,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0xFF,
		0xFF,
		0x05,
		0xAB
};

//
//	Initialization with UART interace
//
SDS011_STATUS Sds011_InitUart(UART_HandleTypeDef *huart)
{
	huart_sds = huart;

	Sds011_SetWorkingPeriod(0);

	HAL_UART_Receive_IT(huart_sds, Sds011_Buffer, 10);	// Receive first message

	return SDS011_OK;
}
#endif

#ifdef SDS011_PWM
//
//	Initialization with PWM interace
//
SDS011_STATUS Sds011_InitPwm(TIM_HandleTypeDef *htim, uint32_t PM2_5_TimerChannel, uint32_t PM10_TimerChannel)
{
	htim_sds = htim;
	Pm2_5_TimChannel_sds = PM2_5_TimerChannel;
	Pm10_TimChannel_sds = PM10_TimerChannel;

	HAL_TIM_Base_Start(htim_sds);
	HAL_TIM_IC_Start_IT(htim_sds, TIM_CHANNEL_1);

	return SDS011_OK;
}
#endif

//
//	Get the PM2.5 measured value
//
SDS011_STATUS Sds011_GetPm2_5(uint16_t *Value)
{
	*Value = Pm2_5;
	return SDS011_OK;
}

//
//	Get the PM10 measured value
//
SDS011_STATUS Sds011_GetPm10(uint16_t *Value)
{
	*Value = Pm10;
	return SDS011_OK;
}

#ifdef SDS011_UART
//
//	Set Working period of SDS011
//	0 - continuous, SDS011 put data every 1 second
//	1 - The power saving cycles:
//		30 seconds working, one time send measured value after that. Then sleep for 'Period' minutes.
//
SDS011_STATUS Sds011_SetWorkingPeriod(uint8_t Period)
{
	if(Period > 30) Period = 30;

	uint8_t i;
	uint8_t buffer[19];
	uint32_t Checksum = 0;

	buffer[0] = 0xAA;
	buffer[1] = 0xB4;
	buffer[2] = 8;
	buffer[3] = 1;
	buffer[4] = Period;
	for(i = 5; i <= 14; i++)
		buffer[i] = 0;
	buffer[15] = 0xFF;
	buffer[16] = 0xFF;
	for(i = 2; i <= 16; i++)
		Checksum += buffer[i];
	buffer[17] = Checksum%256;
	buffer[18] = 0xAB;

	if(HAL_OK == HAL_UART_Transmit(huart_sds, buffer, 19, 20))
		return SDS011_OK;

	return SDS011_ERROR;
}

//
//	Put SDS011 into sleep mode
//
SDS011_STATUS Sds011_SetSleepMode(void)
{
	if(HAL_OK == HAL_UART_Transmit(huart_sds, (uint8_t*)Sds011_SleepCommand, 19, 20))
		return SDS011_OK;

	return SDS011_ERROR;
}

//
//	Wake up SDS011
//
SDS011_STATUS Sds011_WakeUp(void)
{
	uint8_t cmd = 0x01;

	if(HAL_OK == HAL_UART_Transmit(huart_sds, &cmd, 1, 10))
	{
		HAL_UART_Receive_IT(huart_sds, Sds011_Buffer, 10);	// One message
		return SDS011_OK;
	}


	return SDS011_ERROR;
}

//
//	Reat raw data. Helper function.
//
void Sds011_ReadRawData(void)
{
	uint8_t i;
	uint32_t Checksum = 0;

	for(i = 2; i <= 7; i++)
		Checksum += Sds011_Buffer[i];

	if(((Checksum%256) == Sds011_Buffer[8]))
	{
		Pm2_5 = ((Sds011_Buffer[3]<<8) | Sds011_Buffer[2]) / 10;
		Pm10 = ((Sds011_Buffer[5]<<8) | Sds011_Buffer[4]) / 10;
	}
}

//
//	UART HAL Callback. Use it in HAL _weak HAL_UART_RxCpltCallback function
//
void Sds011_UartRxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart == huart_sds)
	{
		if(Sds011_Buffer[0] == 0xAA)	// Header
		{
			if((Sds011_Buffer[1] == SDS011_RAW_DATA_RECEIVE) && (Sds011_Buffer[9] == 0xAB)) // Command byte
			{
				Sds011_ReadRawData();
			}
		}
		HAL_UART_Receive_IT(huart_sds, Sds011_Buffer, 10);
	}
}
#endif

#ifdef SDS011_PWM
//
//	UART HAL Callback. Use it in HAL _weak HAL_TIM_IC_CaptureCallback function
//
void Sds011_TimerCaptureCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == htim_sds->Instance)
	{
		static uint8_t state = 0;
		if(state == 0) // Rising edge - begin
		{
			// Clear counter
			__HAL_TIM_SetCounter(htim_sds, 0);

			// Set edge to falling
			htim->Instance->CCER |= (1<<(Pm10_TimChannel_sds+1));
			HAL_TIM_IC_Start_IT(htim_sds, Pm10_TimChannel_sds);

			// Change state
			state = 1;
		}
		else if(state == 1) // First falling edge
		{
			// Change state only and wait for second edge
			state = 2;
		}
		else // Second falling edge
		{
			// Set edge back to Rising
			htim->Instance->CCER &= ~(1<<(Pm10_TimChannel_sds+1));

			// Get values
			Pm10  = __HAL_TIM_GetCompare(htim_sds, Pm10_TimChannel_sds) - 2;
			Pm2_5 = __HAL_TIM_GetCompare(htim_sds, Pm2_5_TimChannel_sds) - 2;

			// Enable Interrupts
			HAL_TIM_IC_Start_IT(htim_sds, Pm10_TimChannel_sds);
			HAL_TIM_IC_Start_IT(htim_sds, Pm2_5_TimChannel_sds);

			// Back to beginning - wait for next data
			state = 0;
		}
	}
}
#endif
