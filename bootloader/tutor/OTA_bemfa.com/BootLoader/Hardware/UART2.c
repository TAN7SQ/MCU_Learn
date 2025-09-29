#include "stm32f10x.h" // Device header
#include <stdio.h>
#include <stdarg.h>
#include "Address.h"
#include "W25Q64.h"

char Serial2_RxPacket[512]; // 定义接收数据包数组
volatile uint8_t Serial2_RxFlag;		// 定义接收数据包标志位
volatile uint8_t DownloadFlag;
uint16_t addrP = 0;

/**
 * 函    数：串口初始化
 * 参    数：无
 * 返 回 值：无
 */
void Serial2_Init(uint32_t baudRate)
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // 开启USART2的时钟,它属于APB1
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);  // 开启GPIOA的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure); // 将PA2引脚初始化为复用推挽输出

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure); // 将PA3引脚初始化为上拉输入

	/*USART初始化*/
	USART_InitTypeDef USART_InitStructure;											// 定义结构体变量
	USART_InitStructure.USART_BaudRate = baudRate;									// 波特率
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 硬件流控制，不需要
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;					// 模式，发送模式和接收模式均选择
	USART_InitStructure.USART_Parity = USART_Parity_No;								// 奇偶校验，不需要
	USART_InitStructure.USART_StopBits = USART_StopBits_1;							// 停止位，选择1位
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;						// 字长，选择8位
	USART_Init(USART2, &USART_InitStructure);										// 将结构体变量交给USART_Init，配置USART2

	/*中断输出配置*/
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // 开启串口接收中断
	USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); // 开启串口空闲中断

	/*NVIC中断分组*/
	//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);			//配置NVIC分组

	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;					  // 定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;		  // 选择配置NVIC的USART2线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  // 指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // 指定NVIC线路的抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		  // 指定NVIC线路的响应优先级
	NVIC_Init(&NVIC_InitStructure);							  // 将结构体变量交给NVIC_Init，配置NVIC外设

	/*USART使能*/
	USART_Cmd(USART2, ENABLE); // 使能USART2，串口开始运行
}

/**
 * 函    数：串口发送一个字节
 * 参    数：Byte 要发送的一个字节
 * 返 回 值：无
 */
void Serial2_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte); // 将字节数据写入数据寄存器，写入后USART自动生成时序波形
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
		; // 等待发送数据缓冲区为空
	/*下次写入数据寄存器会自动清除发送完成标志位，故此循环后，无需清除标志位*/
}

/**
 * 函    数：串口发送一个数组
 * 参    数：Array 要发送数组的首地址
 * 参    数：Length 要发送数组的长度
 * 返 回 值：无
 */
void Serial2_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i++) // 遍历数组
	{
		Serial2_SendByte(Array[i]); // 依次调用Serial_SendByte发送每个字节数据
	}
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
		; // 等待发送移位寄存器发送完成
}

/**
 * 函    数：串口发送一个字符串
 * 参    数：String 要发送字符串的首地址
 * 返 回 值：无
 */
void Serial2_SendString(char *String)
{
	uint16_t i;
	for (i = 0; String[i] != '\0'; i++) // 遍历字符数组（字符串），遇到字符串结束标志位后停止
	{
		Serial2_SendByte(String[i]); // 依次调用Serial_SendByte发送每个字节数据
	}
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
		; // 等待发送移位寄存器发送完成
}

/**
 * 函    数：次方函数（内部使用）
 * 返 回 值：返回值等于X的Y次方
 */
uint32_t Serial2_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1; // 设置结果初值为1
	while (Y--)			 // 执行Y次
	{
		Result *= X; // 将X累乘到结果
	}
	return Result;
}

/**
 * 函    数：串口发送数字
 * 参    数：Number 要发送的数字，范围：0~4294967295
 * 参    数：Length 要发送数字的长度，范围：0~10
 * 返 回 值：无
 */
void Serial2_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++) // 根据数字长度遍历数字的每一位
	{
		Serial2_SendByte(Number / Serial2_Pow(10, Length - i - 1) % 10 + '0'); // 依次调用Serial_SendByte发送每位数字
	}
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
		; // 等待发送移位寄存器发送完成
}

/**
 * 函    数：自己封装的prinf函数
 * 参    数：format 格式化字符串
 * 参    数：... 可变的参数列表
 * 返 回 值：无
 */
void Serial2_Printf(char *format, ...)
{
	char String[512];			   // 定义字符数组
	va_list arg;				   // 定义可变参数列表数据类型的变量arg
	va_start(arg, format);		   // 从format开始，接收参数列表到arg变量
	vsprintf(String, format, arg); // 使用vsprintf打印格式化字符串和参数列表到字符数组中
	va_end(arg);				   // 结束变量arg
	Serial2_SendString(String);	   // 串口发送字符数组（字符串）
}

/**
 * 函    数：USART2中断函数
 * 参    数：无
 * 返 回 值：无
 * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
 *           函数名为预留的指定名称，可以从启动文件复制
 *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
 */
void USART2_IRQHandler(void)
{
	static uint16_t pRxPacket = 0; // 定义表示当前接收数据位置的静态变量
	uint8_t RxData;

	if (DownloadFlag == 1) // 代码下载模式，将从4G模块收到的字节写入W25Q64
	{
		if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) // 判断是否是USART2的接收事件触发的中断
		{
			RxData = USART_ReceiveData(USART2);					  // 读取数据寄存器，存放在接收的数据变量
			W25Q64_PageProgram(REPO_ADDRESS + addrP, &RxData, 1); // 每收一个字节都写入W25Q64

			TIM_SetCounter(TIM2, 0); // 每接收一个字节，都要将计时器清零
			if (addrP == 0)			 // 第一个字节数据来了,开启定时器
			{
				USART_ITConfig(USART2, USART_IT_IDLE, DISABLE); // 关闭串口空闲中断
				TIM_Cmd(TIM2, ENABLE);
			}
			addrP++; // 数据包的位置自增
		}
	}
	else // 普通串口模式
	{
		if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) // 判断是否是USART2的接收事件触发的中断
		{
			RxData = USART_ReceiveData(USART2); // 读取数据寄存器，存放在接收的数据变量
			if(pRxPacket < 512 - 32) // 防止缓冲区下标溢出
			{
				Serial2_RxPacket[pRxPacket] = RxData; // 将数据存入数据包数组的指定位置
				pRxPacket++;						  // 数据包的位置自增
			}
		}
		else if (USART_GetITStatus(USART2, USART_IT_IDLE) == SET) // 触发中断,同时USART2的空闲位置1,表示数据传输完毕了
		{
			Serial2_RxPacket[pRxPacket] = '\0'; // 将收到的字符数据包添加一个字符串结束标志
			pRxPacket = 0;						// 下标归零

			// 清除标志位, 注意IDLE不能用库函数清零,IDLE会在最后一个数据传输完毕后再一个周期被置1,因此上面的else if不能省略!
			USART2->SR;
			USART2->DR;

			Serial2_RxFlag = 1; // 拿到了一个字符串
		}
	}
}

/**
 * 函    数：TIM2中断函数
 * 参    数：无
 * 返 回 值：无
 * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
 *           函数名为预留的指定名称，可以从启动文件复制
 *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
 */
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) // 判断是否是TIM2的更新事件触发的中断
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除TIM2更新事件的中断标志位
													// 中断标志位必须清除
													// 否则中断将连续不断地触发，导致主程序卡死
		// Tim2进入更新中断认为传输结束
		TIM_Cmd(TIM2, DISABLE);
		DownloadFlag = 0; // 表示完成了代码下载，此时addrP即为接收的字符数
		USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); // 开启串口空闲中断
	}
}
