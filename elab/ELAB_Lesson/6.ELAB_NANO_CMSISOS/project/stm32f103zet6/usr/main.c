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
#include "../../../elab/3rd/Shell/shell.h"
#include "../../../elab/os/cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>
ELAB_TAG("Main");

  /* ==================== [Defines] ========================================== */
  
  /* ==================== [Macros] ============================================ */
  
  /* ==================== [Typedefs] ========================================== */
  
  /* ==================== [Static Prototypes] ========================================== */
  
  /* ==================== [Static Variables] ========================================== */
  
  /* ==================== [Static Functions] ================================== */

int func(int argc, char *argv[])
{
    elog_info("%dparameter(s)\r\n", argc);
    for (int i = 1; i < argc; i++)
    {
        elog_info("%s\r\n", argv[i]);
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), func, func, test);


void test_cmd_shell(void)
{
    elog_info("This is a test command for shell.");
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
test_cmd_shell, test_cmd_shell, "A test command for shell");


int funcParam(int i, char ch, char *str)
{
    elog_info("input int: %d, char: %c, string: %s\r\n", i, ch, str);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), funcParam, funcParam, "A function with parameters(int, char, string)  and return");



void FloatParam(int a, float b,int c,float d)
{
    elog_info("%d, %f, %d, %f \r\n", a, b, c, d );
}
SHELL_EXPORT_CMD_AGENCY(SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
FloatParam, FloatParam, FloatParam,
p1, SHELL_PARAM_FLOAT(p2), p3, SHELL_PARAM_FLOAT(p4));


int gccFloatParam(int argc, char *argv[])
{
  //must use <stdlib.h> !!
   (void)argc;
       for (int i = 0; i <= 4; i++) {
        printf("argv[%d]=%p, str=%s\r\n", i, argv[i], argv[i]);
    }
    float test=11.7;
    printf("test=%f\r\n", test);
    int a;
    float b;
    int c;
    float d;
    sscanf(argv[1], "%d", &a);
    sscanf(argv[2], "%f", &b);
    sscanf(argv[3], "%d", &c);
    sscanf(argv[4], "%f", &d);
    // int a = atoi(argv[1]);
    // float b = atof(argv[2]);
    // int c = atoi(argv[3]);
    // float d = atof(argv[4]);
    printf("%d, %f, %d, %f \r\n", a, b, c, d );
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
gccFloatParam, gccFloatParam, "A function with float parameters in gcc");

void key_hello(void)
{
	elog_info("Hello World\r\n");
}
SHELL_EXPORT_KEY(SHELL_CMD_PERMISSION(0), ESH_KEY_CTRL_PLUS_A, key_hello, key_hello);

int varInt = 0;
SHELL_EXPORT_VAR(SHELL_CMD_TYPE(SHELL_TYPE_VAR_INT)|SHELL_CMD_PERMISSION(0), varInt, &varInt, "An integer variable");

char str[]="test string val";
SHELL_EXPORT_VAR(SHELL_CMD_TYPE(SHELL_TYPE_VAR_STRING)|SHELL_CMD_PERMISSION(0), varStr, str, "A string variable");

Shell *shell;

static bool start_flag=0;
void test_poll_shell(void)
{
shell = shellGetCurrent();
start_flag=1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
test_poll_shell, test_poll_shell, "A polling function for shell");




static void timer_poll(void)
{
  if(start_flag)
  {
    
    char msg[] = "Hello, this is a shell output with end line!\r\n";
    shellWriteEndLine(shell, msg, strlen(msg));
   // elog_info("Hello, this is a shell output with end line!\r\n");
  }
   
} 
POLL_EXPORT(timer_poll, 300);


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

  /* ==================== [Public Functions] ================================== */

void timer_callback(void *param)
{
  (void)param;
  elog_debug("os_timer_test");

}
static const osTimerAttr_t timer_attr_test =
{
    .name = "test_timer",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
};
void os_timer_init(void)
{
osTimerId_t timer=osTimerNew(timer_callback,osTimerPeriodic,NULL,&timer_attr_test);
osTimerStart(timer,1000);
}
INIT_EXPORT(os_timer_init, EXPORT_APP);

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
