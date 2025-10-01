/**
 * @file bsp.c
 * @author ZC (387646983@qq.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-14
 * 
 * 
 */

 /* ==================== [Includes] ========================================== */

 #include "stm32f1xx_hal.h"
 #include "debug_uart.h"
 #include "../../../elab/common/elab_export.h"
 #include "../../../elab/common/elab_log.h"
 #include "../../../elab/3rd/Shell/shell.h"
 /* ==================== [Defines] ========================================== */
ELAB_TAG("BSP");
/* private config ----------------------------------------------------------- */
#define SHELL_POLL_PERIOD_MS                (10)
#define SHELL_BUFFER_SIZE                   (512)


 /* ==================== [Macros] ============================================ */
 
 /* ==================== [Typedefs] ========================================== */
 
 /* ==================== [Static Prototypes] ========================================== */
static Shell shell_uart;
static char shell_uart_buffer[SHELL_BUFFER_SIZE];
 /* ==================== [Static Variables] ========================================== */
 
 /* ==================== [Static Functions] ================================== */
 
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

#if (ELAB_RTOS_CMSIS_OS_EN == 0)
uint32_t elab_time_ms(void)
{
  return HAL_GetTick();
}
#endif

 /* ==================== [Public Functions] ================================== */
 void BSP_Init(void)
 {
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();


  elab_debug_uart_init(115200);
  shell_uart.read=(int16_t (*)(char *, uint16_t))elab_debug_uart_receive;
  shell_uart.write = (int16_t (*)(char *, uint16_t))elab_debug_uart_send;

  shellInit(&shell_uart, shell_uart_buffer, SHELL_BUFFER_SIZE);

	elog_debug("bsp_init");
  /* Initialize all configured peripherals */

 }
INIT_EXPORT(BSP_Init, EXPORT_LEVEL_BSP);


static void shell_poll(void)
{
    char byte;
    while (shell_uart.read && shell_uart.read(&byte, 1) == 1)
    {
        shellHandler(&shell_uart, byte);

    }
   
} 

POLL_EXPORT(shell_poll, SHELL_POLL_PERIOD_MS);


