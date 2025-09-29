/*
魔法棒-Target-IROM1-起始地址 字节数大小
修改魔法棒后要使用"Rebuild all target files"编译
*/
#include "stm32f10x.h"
#include "Key.h"
#include "Tim3.h"
#include "UART1.h"
#include "string.h"
#include "Delay.h"

#define START_ADDRESS 0x08005000

uint8_t Tim3Up = 0;

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
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); // 配置NVIC分组
	
	/*模块初始化*/
	Key_Init();
	Tim3_Init();
	Serial_Init(115200);  // 串口初始化

	// 使用库函数实现点灯
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_13); // 默认高电平

	TIM_Cmd(TIM3, ENABLE); // 计时器使能
	while (1)
	{
		if (Key_GetNum() == 1)
		{
			GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
			Delay_ms(20);//防止短时间内重复执行
		}
		if (Serial_RxFlag == 1) // 如果Debug串口接收到数据包
		{
			Serial_RxFlag = 0; // 处理完成后，需要将接收数据包标志位清零，否则将无法接收后续数据包
			Serial_Printf("Echo:%s\r\n", Serial_RxPacket);
			memset(Serial_RxPacket, 0, 512);
		}
	}
}

/**
 * 函    数：TIM3中断函数
 * 参    数：无
 * 返 回 值：无
 * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
 *           函数名为预留的指定名称，可以从启动文件复制
 *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
 */
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET) // 判断是否是TIM3的更新事件触发的中断
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update); // 清除TIM3更新事件的中断标志位
													// 中断标志位必须清除
													// 否则中断将连续不断地触发，导致主程序卡死
		Tim3Up++;
		Serial_Printf("UserApplication...%d\r\n", Tim3Up);
	}
}
