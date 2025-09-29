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
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); // 配置NVIC为分组4

	/*模块初始化*/
	Key_Init();
	Serial_Init(115200);  // 串口初始化
	Tim2_Init(); // 初始化Tim2
	Serial2_Init(115200); // 4G初始化
	Tim3_Init();

	Serial_SendString("BootLoader started.\r\n");

	uint8_t count = 0;
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE); // 计时器使能

	while (count < 10) // BootLoader执行数秒后进入主程序
	{
		if (Serial_RxFlag == 1) // 如果Debug串口接收到数据包
		{
			Serial_RxFlag = 0; // 处理完成后，需要将接收数据包标志位清零，否则将无法接收后续数据包
			Serial_Printf("Echo:%s\r\n", Serial_RxPacket);
			memset(Serial_RxPacket, 0, 512);
		}

		if (Key_GetNum() == 1) // 启动代码更新
		{
			TIM_Cmd(TIM3, DISABLE);

			// 下载并搬运W25Q64的代码到UserApplication区
			if (codeDownload() == 1)
			{
				codeTransport();
			}
			else
			{
				Serial_SendString("Download failed!\r\n");
				NVIC_SystemReset(); // 软件复位
			}
			break;
		}

		if (tim3Up % (1000 - 1) == 0) // 计数减一防止短周期任务屏蔽长周期任务
		{
			tim3Up++; // 防止在定时器更新周期内重复执行
			count++;
			Serial_Printf("Wait %ds before start.\r\n", count);
		}
	}
	
	/*BootLoader运行结束的善后工作*/
	TIM_Cmd(TIM3, DISABLE);
	TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE); // 关闭TIM3的更新中断
	TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE); // 关闭TIM2的更新中断
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE); // 关闭串口接收中断
	USART_ITConfig(USART1, USART_IT_IDLE, DISABLE); // 关闭串口空闲中断
	USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
	USART_ITConfig(USART2, USART_IT_IDLE, DISABLE);
	SPI_Cmd(SPI1, DISABLE);//关闭SPI1

	// 跳转到用户程序区
	/*
	禁止CPU响应中断，没有真正的去屏蔽中断的触发。
	这个非常重要，如果没有关闭boot下的中断，很容易跳到app后被卡死。
	*/
	__disable_irq();
	/**
	 * @brief 设置主堆栈指针。堆栈的初始化，重新设定栈顶代地址，把栈顶地址设置为用户代码指向的栈顶地址。
	 * @param topOfMainStack主堆栈指针
	 * 将值mainStackPointer分配给MSP（主堆栈指针）Cortex处理器寄存器
	 */
	__set_MSP(*(__IO uint32_t *)START_ADDRESS);
	// 定义一个函数指针，执行时跳转到指定地址
	void (*userResetHandler)(void) = (void (*)(void))(*(__IO uint32_t *)(START_ADDRESS + 4));
	// 跳转到用户复位中断函数，跳完就不会再回到BootLoader了
	userResetHandler();
}
