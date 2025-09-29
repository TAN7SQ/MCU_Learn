#ifndef __UART1_H
#define __UART1_H

#include <stdio.h>

extern char Serial_RxPacket[512];
extern volatile uint8_t Serial_RxFlag;

void Serial_Init(uint32_t baudRate);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t Number, uint8_t Length);
void Serial_Printf(char *format, ...);

#endif
