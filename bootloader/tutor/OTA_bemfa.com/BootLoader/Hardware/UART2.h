#ifndef __UART2_H
#define __UART2_H

#include <stdio.h>

extern char Serial2_RxPacket[512];
extern volatile uint8_t Serial2_RxFlag;
extern volatile uint8_t DownloadFlag;
extern uint16_t addrP;

void Serial2_Init(uint32_t baudRate);
void Serial2_SendByte(uint8_t Byte);
void Serial2_SendArray(uint8_t *Array, uint16_t Length);
void Serial2_SendString(char *String);
void Serial2_SendNumber(uint32_t Number, uint8_t Length);
void Serial2_Printf(char *format, ...);

#endif
