/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* include ------------------------------------------------------------------ */
#include "elab_std.h"


#include "elab_def.h"
#include "elab_export.h"
#include "elab_common.h"
#include "elab_assert.h"
#include <signal.h>
#include "stdio.h"
ELAB_TAG("eLabExport");

#if (ELAB_RTOS_CMSIS_OS_EN != 0) //os
#include "../os/cmsis_os.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* private defines ---------------------------------------------------------- */


#define ELAB_POLL_PERIOD_MAX                        (2592000000)    /* 30 days */

#if defined(__linux__)
#define STR_ENTER                                   "\n"
#else
#define STR_ENTER                                   "\r\n"
#endif

/* private function prototype ----------------------------------------------- */
static void module_null_init(void);
static void _init_func_execute(int8_t level);
static void _get_poll_export_table(void);
static void _get_init_export_table(void);
#if defined(__linux__) || defined(_WIN32)
static void elab_exit(void);
static void _exit_func_execute(int8_t level);
#endif

#if (ELAB_RTOS_CMSIS_OS_EN != 0 )
static void _entry_start_poll(void *para);
#endif

static void _poll_func_execute(void);

/* private variables -------------------------------------------------------- */
INIT_EXPORT(module_null_init, 0);
POLL_EXPORT(module_null_init, (1000 * 60 * 60));

static elab_export_t *export_init_table = NULL;
static uint32_t count_export_init = 0;
static elab_export_t *export_poll_table = NULL;
static uint32_t count_export_poll = 0;
static int8_t export_level_max = INT8_MIN;

#if (ELAB_RTOS_CMSIS_OS_EN != 0)
/**
 * @brief  The thread attribute for testing.
 */
static const osThreadAttr_t thread_attr_export_poll = 
{
    .name = "ThreadStartPoll",
    .attr_bits = osThreadDetached,
    .priority = osPriorityNormal,
    .stack_size = 2048,
};
#endif

/* public function ---------------------------------------------------------- */
/**
  * @brief  eLab unit test exporting function.
  * @retval None
  */
void elab_unit_test(void)
{
    _init_func_execute(EXPORT_UNIT_TEST);
}

#if defined(__linux__) || defined(_WIN32)
/**
  * @brief  Signal handler function on WIN32 or Linux.
  * @retval None
  */
static void signal_handler(int sig)
{
    if (sig == SIGSEGV)
    {
        printf("Elab Signal: SIGSEGV.\n");
    }
    else if (sig == SIGINT)
    {
        printf("Elab Signal: SIGINT.\n");
    }
    else if (sig == SIGTERM)
    {
        printf("Elab Signal: SIGTERM.\n");
    }
    else if (sig == SIGABRT)
    {
        printf("Elab Signal: SIGABRT.\n");
    }
    #if defined(SIGKILL)
    else if (sig == SIGKILL)
    {
        printf("Elab Signal: SIGKILL.\n");
    }
    #endif

    #if defined(SIGHUP)
    else if (sig == SIGHUP)
    {
        printf("Elab Signal: SIGHUP.\n");
    }
    #endif
    
    else
    {
        printf("Elab Signal: %d.\n", sig);
    }
    
    elab_exit();
#if defined(__linux__)
    system("stty echo");
#endif
    printf("\033[0;0m\n");
    exit(-1);
}
#endif

/**
  * @brief  eLab startup function.
  * @retval None
  */
void elab_run(void)
{
#if defined(__linux__) || defined(_WIN32)
    signal(SIGINT, signal_handler);                 /* Ctrl + C*/
    signal(SIGTERM, signal_handler);                /* kill pid */
    signal(SIGABRT, signal_handler);
#if defined(__linux__)
    signal(SIGKILL, signal_handler);                /* kill -9 pid */
    signal(SIGHUP, signal_handler);
#endif
    signal(SIGSEGV, signal_handler);
#endif

    /* Start polling function in metal eLab, or start the RTOS kernel in RTOS 
       eLab. */
    _get_init_export_table();
    _get_poll_export_table();

#if (ELAB_RTOS_CMSIS_OS_EN != 0)

    osKernelInitialize();
    osThreadNew(_entry_start_poll, NULL, &thread_attr_export_poll);
#endif
#if (ELAB_RTOS_CMSIS_OS_EN != 0)

    osKernelStart();

#else
    
    /* Initialize all module in eLab. */
    for (uint8_t level = 0; level <= export_level_max; level ++)
    {
        _init_func_execute(level);
    }
    printf("start with NO OS \r\n");
    /* Start polling function in metal eLab. */
    while (1)
    {
        _poll_func_execute();
    }
#endif
}

/* private function --------------------------------------------------------- */
#if defined(__linux__) || defined(_WIN32)
/**
  * @brief  eLab exit function on WIN32 or Linux.
  * @retval None
  */
static void elab_exit(void)
{
    /* Initialize all module in eLab. */
    for (int32_t level = export_level_max; level >= 0; level --)
    {
        _exit_func_execute(level);
    }
}
#endif

/**
  * @brief  Get the init export table.
  */
static void _get_init_export_table(void)
{ 
    elab_export_t *func_block = (elab_export_t *)&init_module_null_init;
    elab_pointer_t address_last;
    
    while (1)
    {
        address_last = ((elab_pointer_t)func_block - sizeof(elab_export_t));
        elab_export_t *table = (elab_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_INIT ||
            table->magic_tail != EXPORT_ID_INIT)
        {
            break;
        }
        func_block = table;
    }
    export_init_table = func_block;

    uint32_t i = 0;
    while (1)
    {
        if (export_init_table[i].magic_head == EXPORT_ID_INIT &&
            export_init_table[i].magic_tail == EXPORT_ID_INIT)
        {
            if (export_init_table[i].level > export_level_max)
            {
                export_level_max = export_init_table[i].level;
            }
            i ++;
        }
        else
        {
            break;
        }
    }
    count_export_init = i;
}

/**
  * @brief  Get the polling export table.
  */
static void _get_poll_export_table(void)
{
    elab_export_t *func_block = ((elab_export_t *)&poll_module_null_init);
    elab_pointer_t address_last;

    while (1)
    {
        address_last = ((elab_pointer_t)func_block - sizeof(elab_export_t));
        elab_export_t *table = (elab_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_POLL ||
            table->magic_tail != EXPORT_ID_POLL)
        {
            break;
        }
        func_block = table;
    }
    export_poll_table = func_block;

    uint32_t i = 0;
    while (1)
    {
        if (export_poll_table[i].magic_head == EXPORT_ID_POLL &&
            export_poll_table[i].magic_tail == EXPORT_ID_POLL)
        {
            assert_name(export_poll_table[i].period_ms <= ELAB_POLL_PERIOD_MAX,
                        export_poll_table[i].name);
            elab_export_poll_data_t *data =
                (elab_export_poll_data_t *)export_poll_table[i].data;
            data->timeout_ms = elab_time_ms() + export_poll_table[i].period_ms;
            i ++;
        }
        else
        {
            break;
        }
    }
    count_export_poll = i;
}

/**
  * @brief  eLab init exporting function executing.
  * @retval None
  */
static void _init_func_execute(int8_t level)
{
    /* Execute the poll function in the specific level. */
    for (uint32_t i = 0; i < count_export_init; i ++)
    {
        if (export_init_table[i].level == level)
        {
            if (!export_init_table[i].exit)
            {
                if (level != EXPORT_UNIT_TEST)
                {
                    printf("Export init %s." STR_ENTER, export_init_table[i].name);
                }
                ((void (*)(void))export_init_table[i].func)();
            }
        }
    }
}

#if defined(__linux__) || defined(_WIN32)
/**
  * @brief  eLab exit exporting function executing.
  * @retval None
  */
static void _exit_func_execute(int8_t level)
{
    /* Execute the poll function in the specific level. */
    for (uint32_t i = 0; i < count_export_init; i ++)
    {
        if (export_init_table[i].level == level)
        {
            if (export_init_table[i].exit)
            {
                printf("Export exit %s." STR_ENTER, export_init_table[i].name);
                ((void (*)(void))export_init_table[i].func)();
            }
        }
    }
}
#endif

/**
  * @brief  eLab polling exporting function executing.
  * @retval None
  */
static void _poll_func_execute(void)
{
    elab_export_poll_data_t *data;
    
    /* Execute the poll function in the specific level. */
    for (uint32_t i = 0; i < count_export_poll; i ++)
    {
        data = export_poll_table[i].data;

        while (1)
        {
            uint64_t _time = (uint64_t)elab_time_ms();
            if (_time < (uint64_t)data->timeout_ms &&
                ((uint64_t)data->timeout_ms - _time) <=
                    (UINT32_MAX - ELAB_POLL_PERIOD_MAX))
            {
                _time += (UINT32_MAX + 1);
            }

            if (_time >= (uint64_t)data->timeout_ms)
            {
                data->timeout_ms += export_poll_table[i].period_ms;
                ((void (*)(void))export_poll_table[i].func)();
            }
            else
            {
                break;
            }
        }
    }
}

#if (ELAB_RTOS_CMSIS_OS_EN != 0 )
/**
  * @brief  eLab startup and poll function.
  * @retval None
  */
static void _entry_start_poll(void *para)
{

    (void)para;
    /* Initialize all module in eLab. */
    for (uint8_t level = 0; level <= export_level_max; level ++)
    {
        _init_func_execute(level);
    }
    printf("start with CMSIS OS \r\n");
    /* Start polling function in metal eLab. */
    while (1)
    {
        _poll_func_execute();

#if (ELAB_RTOS_CMSIS_OS_EN != 0 )
        osDelay(10); //在linux/win非实时系统下此任务sDelay()时间不能太短 建议>10

#endif
    }
}
#endif

/**
  * @brief  eLab null exporting function.
  * @retval None
  */
static void module_null_init(void)
{
    /* NULL */
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
