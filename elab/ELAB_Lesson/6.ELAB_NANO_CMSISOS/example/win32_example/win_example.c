#include <stdio.h>
#include "../../elab/common/elab_common.h"
#include "../../elab/common/elab_log.h"
#include "../../elab/common/elab_export.h"
#include "../../elab/common/elab_assert.h"
#include "../../elab/os/cmsis_os.h"
#include "../../elab/3rd/Shell/shell.h"
#include "elab_config.h"


ELAB_TAG("win_example");

#define SHELL_POLL_PERIOD_MS                (10)
#define SHELL_BUFFER_SIZE                   (512)

static Shell shell_uart;
static char shell_uart_buffer[SHELL_BUFFER_SIZE];



void test_export(void) {
    elog_info("This is a test export function.");
}
INIT_EXPORT(test_export,EXPORT_APP);

void test_function(int *a) {
    assert(a != NULL);
}


void shell_init(void)
{
  elab_debug_uart_init(115200);
  shell_uart.read=(int16_t (*)(char *, uint16_t))elab_debug_uart_receive;
  shell_uart.write = (int16_t (*)(char *, uint16_t))elab_debug_uart_send;

 shellInit(&shell_uart, shell_uart_buffer, SHELL_BUFFER_SIZE);

 elog_info("Shell init finish");
}
INIT_EXPORT(shell_init,EXPORT_LEVEL_BSP);


static void shell_poll(void)
{
    char byte;
    while (shell_uart.read && shell_uart.read(&byte, 1) == 1)
    {
        shellHandler(&shell_uart, byte);

    }
   
} 
POLL_EXPORT(shell_poll, SHELL_POLL_PERIOD_MS);


// static const osThreadAttr_t attr =
// {
//     .name = "debug_uart",
//     .attr_bits = osThreadDetached,
//     .priority = osPriorityBelowNormal,
//     .stack_size = 2048,
// };
    

// void test_thread()
// {
//     while(1){
// elog_info("Hello eLab!");

// osDelay(1000);
//     }

// }


// void timer_callback(void *param)
// {
//   (void)param;
//   elog_debug("os_timer_test");

// }
// static const osTimerAttr_t timer_attr_test =
// {
//     .name = "test_timer",
//     .attr_bits = 0,
//     .cb_mem = NULL,
//     .cb_size = 0,
// };
// void os_timer_init(void)
// {
// osTimerId_t timer=osTimerNew(timer_callback,osTimerPeriodic,NULL,&timer_attr_test);
// osTimerStart(timer,1000);
// }
// INIT_EXPORT(os_timer_init, EXPORT_APP);

int main() {
    elog_info("Hello eLab!");
    elog_debug("This is a debug message.");
    elog_warn("This is a warning message.");
    elog_error("This is an error message.");
    //osThreadNew(test_thread, NULL, &attr); //os_test

    elab_run();

    return 0;
}
