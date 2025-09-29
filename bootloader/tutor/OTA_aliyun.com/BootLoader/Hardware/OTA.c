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
char version[8];		  // 版本号x.x
char url[300];		  // 更新包的url

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

	memset(Serial2_RxPacket, 0, sizeof(Serial2_RxPacket));
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
查询是否有更新包
参数：无
返回：有返回1，无返回0
*/
uint8_t checkUpdate(void)
{
	// 配置4G模块工作在MQTT模式下
	ATCmd("+++", "a", 500);												   // 退出透传模式时序1
	ATCmd("a", "ok", 500);												   // 退出透传模式时序2
	ATCmd("AT+E=OFF\r\n", "OK", 500);									   // 关闭命令回显
	ATCmd("AT+WKMOD=MQTT,ALI\r\n", "OK", 500);							   // 设置工作模式为MQTT,ALI模式
	ATCmd("AT+ALIREGION=cn-shanghai\r\n", "OK", 500);					   // 设置地域信息
	ATCmd("AT+ALIPRODKEY=k1kh1JQ1d0f\r\n", "OK", 500);					   // 设置产品密钥
	ATCmd("AT+ALIDEVSEC=1b246e6af244fdab7d2655798dce9cb0\r\n", "OK", 500); // 设置设备密钥
	ATCmd("AT+ALIDEVNAME=jkc2-1\r\n", "OK", 500);						   // 设置设备名称
	ATCmd("AT+ALIDEVID=k1kh1JQ1d0f.jkc2-1\r\n", "OK", 500);				   // 设置设备ID
	ATCmd("AT+SOCKRSTIM=10\r\n", "OK", 500);							   // 设置socket重连时间间隔10s
	ATCmd("AT+SOCKRSNUM=60\r\n", "OK", 500);							   // 设置socket最大重连次数
	/*
	设置 MQTT 串口传输模式：分发模式。分发模式下，上报的数据中需要增加该主题的序号，
	模块收到串口数据后会根据序号推送至与之关联的主题。消息格式为：num，<payload>。
	*/
	ATCmd("AT+MQTTMOD=1\r\n", "OK", 500);
	ATCmd("AT+MQTTCFG=60,0\r\n", "OK", 500); // 设置 MQTT 心跳包发送周期60s，不清除缓存标识
	ATCmd("AT+HEARTEN=OFF\r\n", "OK", 500);	 // 心跳包不使能
	ATCmd("AT+MQTTWILL=0\r\n", "OK", 500);	 // 不使用遗嘱
	// MQTT 订阅主题：主题编号<subnum>,订阅使能<suben>,<topic>,服务质量等级<qos>
	ATCmd("AT+MQTTSUBTP=1,1,/ota/device/upgrade/k1kh1JQ1d0f/jkc2-1,0\r\n", "OK", 500);
	ATCmd("AT+MQTTSUBTP=2,1,/sys/k1kh1JQ1d0f/jkc2-1/thing/ota/firmware/get_reply,0\r\n", "OK", 500);
	// MQTT 发布参数：主题编号<pubnum>,发布使能<puben>,<topic>,服务质量等级<qos>,保留消息<retained>
	ATCmd("AT+MQTTPUBTP=1,1,/ota/device/inform/k1kh1JQ1d0f/jkc2-1,0,0\r\n", "OK", 500);
	ATCmd("AT+MQTTPUBTP=2,1,/ota/device/progress/k1kh1JQ1d0f/jkc2-1,0,0\r\n", "OK", 500);
	ATCmd("AT+MQTTPUBTP=3,1,/sys/k1kh1JQ1d0f/jkc2-1/thing/ota/firmware/get,0,0\r\n", "OK", 500);
	ATCmd("AT+S\r\n", "OK", 1000); // 发送保存指令，需约300ms，发送之后模块会自动保存和重启，并进入透传模式
	Delay_s(30);				   // 预留模块启动时间
	Serial_SendString("4G module enters MQTT mode.\r\n");

	// 读取内部FLASH的用户数据区的版本号
	for (uint8_t j = 0; j < 8; j++)
	{
		version[j] = MyFLASH_ReadByte(USER_DATA + j);
	}
	// 首次上电，没有版本号，设置为默认的0.0
	if (version[0] == (char)0xFF)
	{
		memset(version, 0, 8);
		strcpy(version, "0.0");
		uint8_t dataArray[1024] = {0};
		for (uint8_t j = 0; j < 8; j++)
		{
			dataArray[j] = version[j];
		}
		// 先把需要保留的数据读出来
		for (uint16_t j = 8; j < 1024; j++) // j是偏移量，单位是1B，每页1KB容量，有1024个Byte
		{
			dataArray[j] = MyFLASH_ReadByte(USER_DATA + j);
		}
		MyFLASH_ErasePage(USER_DATA); // 整页擦除
		// 写回数据
		for (uint16_t j = 0; j < 1024; j += 2)
		{
			MyFLASH_ProgramHalfWord(USER_DATA + j, dataArray[j] | dataArray[j + 1] << 8);
		}
	}
	// 设备上报OTA模块版本
	Serial2_Printf("1,{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\": \"default\"}}", version);
	Delay_ms(200);
	// 设备请求OTA升级包信息
	Serial_SendString("Request upgrade package information.\r\n");
	if (ATCmd("3,{\"id\": \"2\",\"version\": \"1.0\",\"params\": {\"module\": \"default\"},\"method\": \"thing.ota.firmware.get\"}", "\"url\":", 3000) == 0)
		return 0;

	/*数据分析*/
	// 保存url给codeDownload()函数用
	char *index1 = strstr(Serial2_RxPacket, "ota/");
	if (index1 == NULL)
		return 0;
	char *index2 = strstr(index1, "\",\"md5\"");
	if (index2 == NULL)
		return 0;
	strncpy(url, index1, index2 - index1);
	Serial_Printf("url: %s\r\n", url);
	// 保存版本号
	index1 = strstr(Serial2_RxPacket, "version");
	if (index1 == NULL)
		return 0;
	index2 = strstr(index1, "\",\"");
	if (index2 == NULL)
		return 0;
	strncpy(version, index1 + 10, index2 - (index1 + 10));
	Serial_Printf("version: %s\r\n", version);
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
	ATCmd("+++", "a", 500);														// 退出透传模式时序1
	ATCmd("a", "ok", 500);														// 退出透传模式时序2
	ATCmd("AT+E=OFF\r\n", "OK", 500);											// 关闭命令回显
	ATCmd("AT+WKMOD=HTTPD\r\n", "OK", 500);										// 设置工作模式为 HTTPD 模式
	ATCmd("AT+HTPTP=GET\r\n", "OK", 500);										// 设置 HTTPD 的请求方式
	ATCmd("AT+HTPURL=/\r\n", "OK", 500);										// 设置 HTTP 的请求 URL
	ATCmd("AT+HTPSV=ota-cn-shanghai.iot-thing.aliyuncs.com,80\r\n", "OK", 500); // 设置 HTTP 的请求服务器
	ATCmd("AT+HTPTIM=10\r\n", "OK", 500);										// 设置 HTTP 的请求超时时间
	ATCmd("AT+HTPHD=[0D][0A]\r\n", "OK", 500);									// 设置 HTTP 的请求头信息
	ATCmd("AT+HTPPK=OFF\r\n", "OK", 500);										// 设置是否过滤回复信息包头
	ATCmd("AT+S\r\n", "OK", 1000);												// 发送保存指令，需约300ms，发送之后模块会自动保存和重启，并进入透传模式
	Delay_s(20);																// 预留模块启动时间
	Serial_SendString("4G module enters download mode.\r\n");

	W25Q64_Init(); // W25Q64初始化

	// 扇区擦除，FLASH必须先擦再写！！！因为FLASH只能由1写为0
	for (uint8_t i = 0; i < 11; i++) // i是偏移量，单位是4KB
	{
		W25Q64_SectorErase(REPO_ADDRESS + (i << 12)); // 起始地址加上i左移12位(扇区内地址有12位)得到特定扇区的起始地址
	}
	Serial_SendString("W25Q64 Erase completed.\r\n");

	if (DownloadCmd(url, 6000) == 0) // 给6秒启动下载时间
		return 0;

	/*数据分析*/
	uint8_t httpHead[1000] = {0};
	W25Q64_ReadData(REPO_ADDRESS, httpHead, 1000); // 读出HTTP帧头
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

	// 将新的版本号写入内部FLASH的用户数据区
	uint8_t dataArray[1024] = {0};
	for (uint8_t j = 0; j < 8; j++)
	{
		dataArray[j] = version[j];
	}
	// 先把需要保留的数据读出来
	for (uint16_t j = 8; j < 1024; j++) // j是偏移量，单位是1B，每页1KB容量，有1024个Byte
	{
		dataArray[j] = MyFLASH_ReadByte(USER_DATA + j);
	}
	MyFLASH_ErasePage(USER_DATA); // 整页擦除
	// 写回数据
	for (uint16_t j = 0; j < 1024; j += 2)
	{
		MyFLASH_ProgramHalfWord(USER_DATA + j, dataArray[j] | dataArray[j + 1] << 8);
	}

	Serial_SendString("Program completed.\r\n");
}
