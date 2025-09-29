#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
#include "MyFLASH.h"
#include "UART1.h"
#include "UART2.h"
#include "Address.h"
#include "W25Q64.h"
#include "Tim3.h"
#include "string.h"
#include "Delay.h"

uint16_t fullLen;		  // HTTP响应报文大小
unsigned short binLen;	  // Bin文件大小
uint16_t offset;		  // Bin文件字节流的起始下标偏移量
unsigned long long crc64; // 获取的CRC64ECMA校验码

/*
本地计算并验证CRC64_ECMA校验的值
参数：无
返回：成功返回1，失败返回0
*/
uint8_t CRC64_ECMA(void)
{
	unsigned short length = binLen;
	unsigned long long crc_value = 0xFFFFFFFFFFFFFFFF;
	uint32_t repoAddr = REPO_ADDRESS + offset;
	uint8_t data;
	while (length) // 每个字节都参与运算
	{
		W25Q64_ReadData(repoAddr, &data, 1);
		crc_value ^= data;				// 按位异或XOR
		for (uint8_t i = 0; i < 8; i++) // 右移8位
		{
			if (crc_value & 1)									   // 如果此时crc_value是奇数，逻辑右移1位后与多项式异或
				crc_value = (crc_value >> 1) ^ 0xC96C5795D7870F42; // 多项式POLY为0x42F0E1EBA9EA3693，忽略最高位的1，然后翻转
			else
				crc_value = crc_value >> 1; // 如果此时crc_value是偶数，仅逻辑右移1位
		}
		++repoAddr;
		--length;
	}
	crc_value ^= 0xFFFFFFFFFFFFFFFF; // 结果异或值XOROUT为0xFFFFFFFFFFFFFFFF
	Serial_Printf("local crc64ecma: %llu\r\n", crc_value);
	if (crc_value != crc64)
		return 0;
	return 1;
}

/*
发送AT指令
参数：AT指令，期望回复，等待时间
返回：成功返回1，失败返回0
*/
uint8_t ATCmd(char *cmd, char *recv, uint16_t waitT)
{
	Serial2_SendString(cmd); // 发送

	memset(Serial2_RxPacket, 0, 512);
	Serial2_RxFlag = 0;
	TIM_SetCounter(TIM3, 0);
	tim3Up = 0;
	TIM_Cmd(TIM3, ENABLE); // 计时器使能
	while (Serial2_RxFlag == 0 && tim3Up < waitT)
		;
	TIM_Cmd(TIM3, DISABLE);

	if (tim3Up >= waitT) // 等待超时
	{
		Serial_Printf("%s TIMEOUT!\r\n", cmd);
		return 0;
	}

	Serial2_RxFlag = 0;
	if (strstr(Serial2_RxPacket, recv) == NULL) // 不是期望结果
	{
		Serial_Printf("%s FAILED!\r\n %s\r\n", cmd, Serial2_RxPacket);
		return 0;
	}

	Serial_Printf("%s SUCCEED!\r\n", cmd);
	return 1;
}

/*
发送下载指令
参数：下载指令，等待时间
返回：成功返回1，失败返回0
*/
uint8_t DownloadCmd(char *cmd, uint16_t waitT)
{
	DownloadFlag = 1;		 // 下载标志置1，串口进入下载模式
	Serial2_SendString(cmd); // 发送

	TIM_SetCounter(TIM3, 0);
	tim3Up = 0;
	TIM_Cmd(TIM3, ENABLE); // 计时器使能
	while (DownloadFlag == 1 && tim3Up < waitT)
		;
	TIM_Cmd(TIM3, DISABLE);

	if (tim3Up >= waitT) // 等待超时
	{
		DownloadFlag = 0; // 重置下载标志
		addrP = 0;		  // 重置指针
		Serial_SendString("Download TIMEOUT!\r\n");
		return 0;
	}
	if (addrP < 380) // 数据不全
	{
		DownloadFlag = 0; // 重置下载标志

		Serial_Printf("Download %d Byte, but INCOMPLETE!\r\n", addrP);
		addrP = 0; // 重置指针
		return 0;
	}

	fullLen = addrP;
	Serial_Printf("Download %d Byte SUCCEED!\r\n", fullLen);
	addrP = 0; // 重置指针
	return 1;
}

/*
代码下载函数
参数：无
返回：成功返回1，失败返回0
*/
uint8_t codeDownload(void)
{
	// 配置4G模块工作在HTTPD模式下
	ATCmd("+++", "a", 500);							   // 退出透传模式时序1
	ATCmd("a", "ok", 500);							   // 退出透传模式时序2
	ATCmd("AT+E=OFF\r\n", "OK", 500);				   // 关闭命令回显
	ATCmd("AT+WKMOD=HTTPD\r\n", "OK", 500);			   // 设置工作模式为 HTTPD 模式
	ATCmd("AT+HTPTP=GET\r\n", "OK", 500);			   // 设置 HTTPD 的请求方式
	ATCmd("AT+HTPURL=/\r\n", "OK", 500);			   // 设置 HTTP 的请求 URL
	ATCmd("AT+HTPSV=bin.bemfa.com,80\r\n", "OK", 500); // 设置 HTTP 的请求服务器
	ATCmd("AT+HTPTIM=10\r\n", "OK", 500);			   // 设置 HTTP 的请求超时时间
	ATCmd("AT+HTPHD=[0D][0A]\r\n", "OK", 500);		   // 设置 HTTP 的请求头信息
	ATCmd("AT+HTPPK=OFF\r\n", "OK", 500);			   // 设置是否过滤回复信息包头
	ATCmd("AT+S\r\n", "OK", 1000);					   // 发送保存指令，需约300ms，发送之后模块会自动保存和重启，并进入透传模式
	Delay_s(20);									   // 预留模块启动时间
	Serial_SendString("4G module is ready.\r\n");

	W25Q64_Init(); // W25Q64初始化

	// 扇区擦除，FLASH必须先擦再写！！！因为FLASH只能由1写为0
	for (uint8_t i = 0; i < 11; i++) // i是偏移量，单位是4KB
	{
		W25Q64_SectorErase(REPO_ADDRESS + (i << 12)); // 起始地址加上i左移12位(扇区内地址有12位)得到特定扇区的起始地址
	}
	Serial_SendString("W25Q64 Erase completed.\r\n");

	if (DownloadCmd("b/1BcNzVkYWQxMmU1ZjJlNDZhODlmYWNkMjEyMjViZmNlMTk=otaServer010.bin", 6000) == 0) // 给6秒启动下载时间
		return 0;

	// 文件校验部分
	uint8_t httpHead[400] = {0};
	W25Q64_ReadData(REPO_ADDRESS, httpHead, 400); // 读出前400个字节
	// 获得Bin文件大小
	char *index = strstr((char *)httpHead, "Content-Length");
	if (index == NULL)
		return 0;
	sscanf(index + 16, "%hu", &binLen);
	Serial_Printf("Content-Length: %hu\r\n", binLen);
	// 获得CRC64ECMA校验码
	index = strstr((char *)httpHead, "crc64ecma");
	if (index == NULL)
		return 0;
	sscanf(index + 11, "%llu\r\n", &crc64);
	Serial_Printf("crc64ecma: %llu\r\n", crc64);
	// 获得Bin文件字节流的起始下标偏移量
	index = strstr((char *)httpHead, "\r\n\r\n");
	if (index == NULL)
		return 0;
	offset = (uint8_t *)index + 4 - httpHead; // 两指针相减结果为下标之差
	Serial_Printf("offset: %d\r\n", offset);
	// 检查Bin文件大小是否匹配
	if (fullLen - offset != binLen)
		return 0;
	// 本地计算并验证CRC64_ECMA校验的值
	if (CRC64_ECMA() == 0)
		return 0;

	return 1;
}

// 代码搬运函数
void codeTransport(void)
{
	Serial_SendString("Update started.\r\n");
	// 擦除UserApplication区
	for (uint8_t i = 0; i < 43; i++) // i是偏移量，单位是1KB
	{
		MyFLASH_ErasePage(START_ADDRESS + (i << 10)); // 起始地址加上i左移10位(页内地址有10位)得到特定页的起始地址
	}
	Serial_SendString("Erase completed.\r\n");

	// 搬运W25Q64的代码到UserApplication区
	uint32_t userPageAddr = 0;
	uint32_t repoAddr = 0;
	uint8_t halfWord[2] = {0};		 // halfWord[0]是低字节，halfWord[1]是高字节
	for (uint8_t i = 0; i < 43; i++) // i是偏移量，单位是1KB
	{
		userPageAddr = START_ADDRESS + (i << 10);
		repoAddr = REPO_ADDRESS + (i << 10) + offset;
		for (uint16_t j = 0; j < 512; j++) // j是偏移量，单位是2B，每页1KB容量，有512个半字(16bit)
		{								   // 起始地址加上j左移1位(字内地址有1位)得到特定半字的起始地址
			W25Q64_ReadData(repoAddr + (j << 1), halfWord, 2);
			MyFLASH_ProgramHalfWord(userPageAddr + (j << 1), halfWord[0] | halfWord[1] << 8);
		}
		// 坑点：如果用字编程，变量j不要用uint8_t，虽然1页刚好有256字，但j到255+1溢出会导致程序卡死
	}

	Serial_SendString("Program completed.\r\n");
}
