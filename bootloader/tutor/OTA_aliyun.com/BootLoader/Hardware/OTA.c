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

uint16_t fullLen;		  // HTTP��Ӧ���Ĵ�С
unsigned short binLen;	  // Bin�ļ���С
uint16_t offset;		  // Bin�ļ��ֽ�������ʼ�±�ƫ����
unsigned long long crc64; // ��ȡ��CRC64ECMAУ����
char version[8];		  // �汾��x.x
char url[300];		  // ���°���url

/*
���ؼ��㲢��֤CRC64_ECMAУ���ֵ
��������
���أ��ɹ�����1��ʧ�ܷ���0
*/
uint8_t CRC64_ECMA(void)
{
	unsigned short length = binLen;
	unsigned long long crc_value = 0xFFFFFFFFFFFFFFFF;
	uint32_t repoAddr = REPO_ADDRESS + offset;
	uint8_t data;
	while (length) // ÿ���ֽڶ���������
	{
		W25Q64_ReadData(repoAddr, &data, 1);
		crc_value ^= data;				// ��λ���XOR
		for (uint8_t i = 0; i < 8; i++) // ����8λ
		{
			if (crc_value & 1)									   // �����ʱcrc_value���������߼�����1λ�������ʽ���
				crc_value = (crc_value >> 1) ^ 0xC96C5795D7870F42; // ����ʽPOLYΪ0x42F0E1EBA9EA3693���������λ��1��Ȼ��ת
			else
				crc_value = crc_value >> 1; // �����ʱcrc_value��ż�������߼�����1λ
		}
		++repoAddr;
		--length;
	}
	crc_value ^= 0xFFFFFFFFFFFFFFFF; // ������ֵXOROUTΪ0xFFFFFFFFFFFFFFFF
	Serial_Printf("local crc64ecma: %llu\r\n", crc_value);
	if (crc_value != crc64)
		return 0;
	return 1;
}

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

/*
��������ָ��
����������ָ��ȴ�ʱ��
���أ��ɹ�����1��ʧ�ܷ���0
*/
uint8_t DownloadCmd(char *cmd, uint16_t waitT)
{
	DownloadFlag = 1;		 // ���ر�־��1�����ڽ�������ģʽ
	Serial2_SendString(cmd); // ����

	TIM_SetCounter(TIM3, 0);
	tim3Up = 0;
	TIM_Cmd(TIM3, ENABLE); // ��ʱ��ʹ��
	while (DownloadFlag == 1 && tim3Up < waitT)
		;
	TIM_Cmd(TIM3, DISABLE);

	if (tim3Up >= waitT) // �ȴ���ʱ
	{
		DownloadFlag = 0; // �������ر�־
		addrP = 0;		  // ����ָ��
		Serial_SendString("Download TIMEOUT!\r\n");
		return 0;
	}
	if (addrP < 380) // ���ݲ�ȫ
	{
		DownloadFlag = 0; // �������ر�־

		Serial_Printf("Download %d Byte, but INCOMPLETE!\r\n", addrP);
		addrP = 0; // ����ָ��
		return 0;
	}

	fullLen = addrP;
	Serial_Printf("Download %d Byte SUCCEED!\r\n", fullLen);
	addrP = 0; // ����ָ��
	return 1;
}

/*
��ѯ�Ƿ��и��°�
��������
���أ��з���1���޷���0
*/
uint8_t checkUpdate(void)
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
	// �״��ϵ磬û�а汾�ţ�����ΪĬ�ϵ�0.0
	if (version[0] == (char)0xFF)
	{
		memset(version, 0, 8);
		strcpy(version, "0.0");
		uint8_t dataArray[1024] = {0};
		for (uint8_t j = 0; j < 8; j++)
		{
			dataArray[j] = version[j];
		}
		// �Ȱ���Ҫ���������ݶ�����
		for (uint16_t j = 8; j < 1024; j++) // j��ƫ��������λ��1B��ÿҳ1KB��������1024��Byte
		{
			dataArray[j] = MyFLASH_ReadByte(USER_DATA + j);
		}
		MyFLASH_ErasePage(USER_DATA); // ��ҳ����
		// д������
		for (uint16_t j = 0; j < 1024; j += 2)
		{
			MyFLASH_ProgramHalfWord(USER_DATA + j, dataArray[j] | dataArray[j + 1] << 8);
		}
	}
	// �豸�ϱ�OTAģ��汾
	Serial2_Printf("1,{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\": \"default\"}}", version);
	Delay_ms(200);
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

/*
�������غ���
��������
���أ��ɹ�����1��ʧ�ܷ���0
*/
uint8_t codeDownload(void)
{
	// ����4Gģ�鹤����HTTPDģʽ��
	ATCmd("+++", "a", 500);														// �˳�͸��ģʽʱ��1
	ATCmd("a", "ok", 500);														// �˳�͸��ģʽʱ��2
	ATCmd("AT+E=OFF\r\n", "OK", 500);											// �ر��������
	ATCmd("AT+WKMOD=HTTPD\r\n", "OK", 500);										// ���ù���ģʽΪ HTTPD ģʽ
	ATCmd("AT+HTPTP=GET\r\n", "OK", 500);										// ���� HTTPD ������ʽ
	ATCmd("AT+HTPURL=/\r\n", "OK", 500);										// ���� HTTP ������ URL
	ATCmd("AT+HTPSV=ota-cn-shanghai.iot-thing.aliyuncs.com,80\r\n", "OK", 500); // ���� HTTP �����������
	ATCmd("AT+HTPTIM=10\r\n", "OK", 500);										// ���� HTTP ������ʱʱ��
	ATCmd("AT+HTPHD=[0D][0A]\r\n", "OK", 500);									// ���� HTTP ������ͷ��Ϣ
	ATCmd("AT+HTPPK=OFF\r\n", "OK", 500);										// �����Ƿ���˻ظ���Ϣ��ͷ
	ATCmd("AT+S\r\n", "OK", 1000);												// ���ͱ���ָ���Լ300ms������֮��ģ����Զ������������������͸��ģʽ
	Delay_s(20);																// Ԥ��ģ������ʱ��
	Serial_SendString("4G module enters download mode.\r\n");

	W25Q64_Init(); // W25Q64��ʼ��

	// ����������FLASH�����Ȳ���д��������ΪFLASHֻ����1дΪ0
	for (uint8_t i = 0; i < 11; i++) // i��ƫ��������λ��4KB
	{
		W25Q64_SectorErase(REPO_ADDRESS + (i << 12)); // ��ʼ��ַ����i����12λ(�����ڵ�ַ��12λ)�õ��ض���������ʼ��ַ
	}
	Serial_SendString("W25Q64 Erase completed.\r\n");

	if (DownloadCmd(url, 6000) == 0) // ��6����������ʱ��
		return 0;

	/*���ݷ���*/
	uint8_t httpHead[1000] = {0};
	W25Q64_ReadData(REPO_ADDRESS, httpHead, 1000); // ����HTTP֡ͷ
	// ���Bin�ļ���С
	char *index = strstr((char *)httpHead, "Content-Length");
	if (index == NULL)
		return 0;
	sscanf(index + 16, "%hu", &binLen);
	Serial_Printf("Content-Length: %hu\r\n", binLen);
	// ���CRC64ECMAУ����
	index = strstr((char *)httpHead, "crc64ecma");
	if (index == NULL)
		return 0;
	sscanf(index + 11, "%llu\r\n", &crc64);
	Serial_Printf("crc64ecma: %llu\r\n", crc64);
	// ���Bin�ļ��ֽ�������ʼ�±�ƫ����
	index = strstr((char *)httpHead, "\r\n\r\n");
	if (index == NULL)
		return 0;
	offset = (uint8_t *)index + 4 - httpHead; // ��ָ��������Ϊ�±�֮��
	Serial_Printf("offset: %d\r\n", offset);
	// ���Bin�ļ���С�Ƿ�ƥ��
	if (fullLen - offset != binLen)
		return 0;
	// ���ؼ��㲢��֤CRC64_ECMAУ���ֵ
	if (CRC64_ECMA() == 0)
		return 0;

	return 1;
}

// ������˺���
void codeTransport(void)
{
	Serial_SendString("Update started.\r\n");
	// ����UserApplication��
	for (uint8_t i = 0; i < 43; i++) // i��ƫ��������λ��1KB
	{
		MyFLASH_ErasePage(START_ADDRESS + (i << 10)); // ��ʼ��ַ����i����10λ(ҳ�ڵ�ַ��10λ)�õ��ض�ҳ����ʼ��ַ
	}
	Serial_SendString("Erase completed.\r\n");

	// ����W25Q64�Ĵ��뵽UserApplication��
	uint32_t userPageAddr = 0;
	uint32_t repoAddr = 0;
	uint8_t halfWord[2] = {0};		 // halfWord[0]�ǵ��ֽڣ�halfWord[1]�Ǹ��ֽ�
	for (uint8_t i = 0; i < 43; i++) // i��ƫ��������λ��1KB
	{
		userPageAddr = START_ADDRESS + (i << 10);
		repoAddr = REPO_ADDRESS + (i << 10) + offset;
		for (uint16_t j = 0; j < 512; j++) // j��ƫ��������λ��2B��ÿҳ1KB��������512������(16bit)
		{								   // ��ʼ��ַ����j����1λ(���ڵ�ַ��1λ)�õ��ض����ֵ���ʼ��ַ
			W25Q64_ReadData(repoAddr + (j << 1), halfWord, 2);
			MyFLASH_ProgramHalfWord(userPageAddr + (j << 1), halfWord[0] | halfWord[1] << 8);
		}
		// �ӵ㣺������ֱ�̣�����j��Ҫ��uint8_t����Ȼ1ҳ�պ���256�֣���j��255+1����ᵼ�³�����
	}

	// ���µİ汾��д���ڲ�FLASH���û�������
	uint8_t dataArray[1024] = {0};
	for (uint8_t j = 0; j < 8; j++)
	{
		dataArray[j] = version[j];
	}
	// �Ȱ���Ҫ���������ݶ�����
	for (uint16_t j = 8; j < 1024; j++) // j��ƫ��������λ��1B��ÿҳ1KB��������1024��Byte
	{
		dataArray[j] = MyFLASH_ReadByte(USER_DATA + j);
	}
	MyFLASH_ErasePage(USER_DATA); // ��ҳ����
	// д������
	for (uint16_t j = 0; j < 1024; j += 2)
	{
		MyFLASH_ProgramHalfWord(USER_DATA + j, dataArray[j] | dataArray[j + 1] << 8);
	}

	Serial_SendString("Program completed.\r\n");
}
