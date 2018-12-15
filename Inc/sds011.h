/*
 * sds011.h
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

#ifndef SDS011_H_
#define SDS011_H_

//
//	Choose used interface
//
#define SDS011_UART
//#define	SDS011_PWM

#define SDS011_BUFFER_SIZE	10

#define SDS011_RAW_DATA_RECEIVE 0xC0
#define SDS011_REPLY			0xC5
#define SDS011_SEND_COMMAND		0xB4

typedef enum
{
	SDS011_OK 		= 0,
	SDS011_ERROR	= 1
}SDS011_SATUS;

#ifdef SDS011_UART
SDS011_SATUS Sds011_InitUart(UART_HandleTypeDef *huart);
#endif

#ifdef SDS011_PWM
SDS011_SATUS Sds011_InitPwm(TIM_HandleTypeDef *htim, uint32_t PM2_5_TimerChannel, uint32_t PM10_TimerChannel);
#endif

#ifdef SDS011_UART
SDS011_SATUS Sds011_SetWorkingPeriod(uint8_t Period);
SDS011_SATUS Sds011_SetSleepMode(void);
SDS011_SATUS Sds011_WakeUp(void);
#endif

SDS011_SATUS Sds011_GetPm2_5(uint16_t *Value);
SDS011_SATUS Sds011_GetPm10(uint16_t *Value);

#ifdef SDS011_UART
void Sds011_UartRxCpltCallback(UART_HandleTypeDef *huart);
#endif

#ifdef SDS011_PWM
void Sds011_TimerCaptureCallback(TIM_HandleTypeDef *htim);
#endif

#endif /* SDS011_H_ */
