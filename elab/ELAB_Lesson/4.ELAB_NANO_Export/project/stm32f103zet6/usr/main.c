/**
 * @file main.c
 * @author ZC (387646983@qq.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-14
 * 
 * 
 */
  /* ==================== [Includes] ========================================== */
#include "../../../elab/elib/elib_queue.h"
#include "../../../elab/common/elab_export.h"
#include "../../../elab/common/elab_log.h"
#include <stdio.h>
ELAB_TAG("Main");

void test_bsp_init(void)
{
elog_debug(" test_bsp_init\r\n");
}

INIT_EXPORT(test_bsp_init, EXPORT_LEVEL_BSP);


void test_driver_init(void)
{
    elog_debug(" test_driver_init\r\n");
}
INIT_EXPORT(test_driver_init, EXPORT_DRVIVER);



void test_app_init(void)
{
    elog_debug(" test_usr_init\r\n");
}
INIT_EXPORT(test_app_init, EXPORT_APP);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

 elab_run();

  while (1)
  {

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
