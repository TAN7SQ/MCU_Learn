/**
 * @file elab_common.h
 * @author ZC (387646983@qq.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-17
 * 
 * 
 */

 
 #ifndef ELAB_COMMON_H 
 #define ELAB_COMMON_H 
 
 /* ==================== [Includes] ========================================== */
 
 #include "elab_std.h"
 #include "elab_def.h"
 #include "elab_config.h" //config配置文件，需要用户自己定义
 #ifdef __cplusplus
 extern "C"{
 #endif
 
 /* ==================== [Defines] ========================================== */

#if (ELAB_RTOS_CMSIS_OS_EN != 0)
#include "../os/cmsis_os.h"
#endif

 /* ==================== [Typedefs] ========================================== */
 
 /* ==================== [Global Prototypes] ================================== */
uint32_t elab_time_ms(void);
void elab_debug_uart_init(uint32_t baudrate);
int16_t elab_debug_uart_send(void *buffer, uint16_t size);
int16_t elab_debug_uart_receive(void *buffer, uint16_t size);
 /* Memory management related. */
void *elab_malloc(uint32_t size);
void elab_free(void *memory);
 /* ==================== [Macros] ============================================ */



 #ifdef __cplusplus
 }
 #endif
 
 #endif	/* ELAB_COMMON_H */
