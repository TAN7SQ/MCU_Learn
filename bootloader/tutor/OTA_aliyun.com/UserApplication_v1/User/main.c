/*
魔法棒-Target-IROM1-起始地址 字节数大小
修改魔法棒后要使用"Rebuild all target files"编译

中断优先级表
名称			抢占优先级	响应优先级
TIM2-Download		2			0
TIM3-Counter		1			0
USART1-Serial		4			0
USART2-4G			3			0
*/
#include "stm32f10x.h"
#include "OTA.h"
#include "UART1.h"
#include "Tim2.h"
#include "UART2.h"
#include "Delay.h"
#include "Key.h"
#include "Tim3.h"
#include "Address.h"
#include "string.h"

int main(void)
{
	/**
	 *@brief设置矢量表的位置和偏移量。
	 *@param NVIC_VectTab：指定矢量表是在RAM还是FLASH内存中。
	 *此参数可以是以下值之一：
	 *@arg NVIC_VectTab_RAM
	 *@arg NVIC_VectTab_FLASH
	 *@param Offset：矢量表基本偏移字段。此值必须是0x200(512Byte)的倍数。
	 *@retval无
	 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, START_ADDRESS);
	__enable_irq(); // 因为BootLoader里面禁用了，这里使能全局中断

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); // 配置NVIC为分组4

	/*模块初始化*/
	Key_Init();
	Serial_Init(115200);  // 串口初始化
	Tim2_Init();		  // 初始化Tim2
	Serial2_Init(115200); // 4G初始化
	Tim3_Init();
	mqttInit();

	// 使用库函数实现点灯
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_13); // 默认高电平

	Serial_SendString("UserApplication started.\r\n");

	TIM_SetCounter(TIM3, 0);
	tim3Up = 0;
	TIM_Cmd(TIM3, ENABLE); // 计时器使能

	while (1)
	{
		if (Key_GetNum() == 1)
		{
			GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
		}

		if (Serial_RxFlag == 1) // 如果Debug串口接收到数据包
		{
			Serial_RxFlag = 0; // 处理完成后，需要将接收数据包标志位清零，否则将无法接收后续数据包
			Serial_Printf("Echo:%s\r\n", Serial_RxPacket);
			memset(Serial_RxPacket, 0, sizeof(Serial_RxPacket));
		}

		if (tim3Up % (30000 - 1) == 0) // 周期性检测更新。计数减一防止短周期任务屏蔽长周期任务
		{
			uint16_t tim3Mem = tim3Up + 1; // 暂存当前的时间戳。防止在定时器更新周期内重复执行
			TIM_Cmd(TIM3, DISABLE); // 暂停定时器，因为后续检测更新需要借用该定时器
			
			if (checkUpdate() == 1)
			{
				Serial_SendString("Discover a new version!\r\n");
				NVIC_SystemReset(); // 软件复位
			}
			
			TIM_SetCounter(TIM3, 0);
			tim3Up = tim3Mem;
			TIM_Cmd(TIM3, ENABLE); // 计时器使能
		}
	}
}
