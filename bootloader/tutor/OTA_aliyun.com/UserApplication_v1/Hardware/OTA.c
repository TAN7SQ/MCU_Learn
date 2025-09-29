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

char version[8]; // �汾��x.x
char url[300];   // ���°���url

/*
����ATָ��
������ATָ������ظ����ȴ�ʱ��
���أ��ɹ�����1��ʧ�ܷ���0
*/
uint8_t ATCmd(char *cmd, char *recv, uint16_t waitT)
{
	Serial2_SendString(cmd); // ����

	memset(Serial2_RxPacket, 0, sizeof(Serial2_RxPacket));
	Serial2_RxFlag = 0;
	TIM_SetCounter(TIM3, 0);
	tim3Up = 0;
	TIM_Cmd(TIM3, ENABLE); // ��ʱ��ʹ��
	while (Serial2_RxFlag == 0 && tim3Up < waitT)
		;
	TIM_Cmd(TIM3, DISABLE);

	if (tim3Up >= waitT) // �ȴ���ʱ
	{
		Serial_Printf("%s TIMEOUT!\r\n", cmd);
		return 0;
	}

	Serial2_RxFlag = 0;
	if (strstr(Serial2_RxPacket, recv) == NULL) // �����������
	{
		Serial_Printf("%s FAILED!\r\n %s\r\n", cmd, Serial2_RxPacket);
		return 0;
	}

	Serial_Printf("%s SUCCEED!\r\n", cmd);
	return 1;
}

void mqttInit(void)
{
	// ����4Gģ�鹤����MQTTģʽ��
	ATCmd("+++", "a", 500);												   // �˳�͸��ģʽʱ��1
	ATCmd("a", "ok", 500);												   // �˳�͸��ģʽʱ��2
	ATCmd("AT+E=OFF\r\n", "OK", 500);									   // �ر��������
	ATCmd("AT+WKMOD=MQTT,ALI\r\n", "OK", 500);							   // ���ù���ģʽΪMQTT,ALIģʽ
	ATCmd("AT+ALIREGION=cn-shanghai\r\n", "OK", 500);					   // ���õ�����Ϣ
	ATCmd("AT+ALIPRODKEY=k1kh1JQ1d0f\r\n", "OK", 500);					   // ���ò�Ʒ��Կ
	ATCmd("AT+ALIDEVSEC=1b246e6af244fdab7d2655798dce9cb0\r\n", "OK", 500); // �����豸��Կ
	ATCmd("AT+ALIDEVNAME=jkc2-1\r\n", "OK", 500);						   // �����豸����
	ATCmd("AT+ALIDEVID=k1kh1JQ1d0f.jkc2-1\r\n", "OK", 500);				   // �����豸ID
	ATCmd("AT+SOCKRSTIM=10\r\n", "OK", 500);							   // ����socket����ʱ����10s
	ATCmd("AT+SOCKRSNUM=60\r\n", "OK", 500);							   // ����socket�����������
	/*
	���� MQTT ���ڴ���ģʽ���ַ�ģʽ���ַ�ģʽ�£��ϱ�����������Ҫ���Ӹ��������ţ�
	ģ���յ��������ݺ����������������֮���������⡣��Ϣ��ʽΪ��num��<payload>��
	*/
	ATCmd("AT+MQTTMOD=1\r\n", "OK", 500);
	ATCmd("AT+MQTTCFG=60,0\r\n", "OK", 500); // ���� MQTT ��������������60s������������ʶ
	ATCmd("AT+HEARTEN=OFF\r\n", "OK", 500);	 // ��������ʹ��
	ATCmd("AT+MQTTWILL=0\r\n", "OK", 500);	 // ��ʹ������
	// MQTT �������⣺������<subnum>,����ʹ��<suben>,<topic>,���������ȼ�<qos>
	ATCmd("AT+MQTTSUBTP=1,1,/ota/device/upgrade/k1kh1JQ1d0f/jkc2-1,0\r\n", "OK", 500);
	ATCmd("AT+MQTTSUBTP=2,1,/sys/k1kh1JQ1d0f/jkc2-1/thing/ota/firmware/get_reply,0\r\n", "OK", 500);
	// MQTT ����������������<pubnum>,����ʹ��<puben>,<topic>,���������ȼ�<qos>,������Ϣ<retained>
	ATCmd("AT+MQTTPUBTP=1,1,/ota/device/inform/k1kh1JQ1d0f/jkc2-1,0,0\r\n", "OK", 500);
	ATCmd("AT+MQTTPUBTP=2,1,/ota/device/progress/k1kh1JQ1d0f/jkc2-1,0,0\r\n", "OK", 500);
	ATCmd("AT+MQTTPUBTP=3,1,/sys/k1kh1JQ1d0f/jkc2-1/thing/ota/firmware/get,0,0\r\n", "OK", 500);
	ATCmd("AT+S\r\n", "OK", 1000); // ���ͱ���ָ���Լ300ms������֮��ģ����Զ������������������͸��ģʽ
	Delay_s(30);				   // Ԥ��ģ������ʱ��
	Serial_SendString("4G module enters MQTT mode.\r\n");

	// ��ȡ�ڲ�FLASH���û��������İ汾��
	for (uint8_t j = 0; j < 8; j++)
	{
		version[j] = MyFLASH_ReadByte(USER_DATA + j);
	}
	// �豸�ϱ�OTAģ��汾
	Serial2_Printf("1,{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\": \"default\"}}", version);
	Delay_ms(200);
}

/*
��ѯ�Ƿ��и��°�
��������
���أ��з���1���޷���0
*/
uint8_t checkUpdate(void)
{
	// �豸����OTA��������Ϣ
	Serial_SendString("Request upgrade package information.\r\n");
	if (ATCmd("3,{\"id\": \"2\",\"version\": \"1.0\",\"params\": {\"module\": \"default\"},\"method\": \"thing.ota.firmware.get\"}", "\"url\":", 3000) == 0)
		return 0;

	/*���ݷ���*/
	// ����url��codeDownload()������
	char *index1 = strstr(Serial2_RxPacket, "ota/");
	if (index1 == NULL)
		return 0;
	char *index2 = strstr(index1, "\",\"md5\"");
	if (index2 == NULL)
		return 0;
	strncpy(url, index1, index2 - index1);
	Serial_Printf("url: %s\r\n", url);
	// ����汾��
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
