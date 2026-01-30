#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "stm32l4xx_hal.h"

#define CMD_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128
#define RX_BUFFER_SIZE 64
#define RX_QUEUE_LEN 64

void CommandParser_Run(void);
void CommandParser_Init(void);
void CommandParser_RxCallback(UART_HandleTypeDef* huart);

#endif
