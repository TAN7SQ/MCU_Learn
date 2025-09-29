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

char version[8]; // 版本号x.x
char url[300];   // 更新包的url

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

void mqttInit(void)
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
	// 设备上报OTA模块版本
	Serial2_Printf("1,{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\": \"default\"}}", version);
	Delay_ms(200);
}

/*
查询是否有更新包
参数：无
返回：有返回1，无返回0
*/
uint8_t checkUpdate(void)
{
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
