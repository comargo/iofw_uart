#ifndef IOFW_UART_H
#define IOFW_UART_H

#include <stm32f4xx_hal.h>

/**************
 * The desired UART have to be initialized:
 **************/

void iofw_uart_register();


// IRQ Handlers.
// Please call them if you plan to override USARTx_IRQHandler
//   and keep /dev/ttySx working
void iofw_uart_USART1_IRQHandler(void);
void iofw_uart_USART2_IRQHandler(void);
void iofw_uart_USART6_IRQHandler(void);

// UART Complete callback
// Please call them if you plan to override HAL_UART_RxCpltCallback
//   and keep /dev/ttySx working
void iofw_uart_HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif//IOFW_UART_H
