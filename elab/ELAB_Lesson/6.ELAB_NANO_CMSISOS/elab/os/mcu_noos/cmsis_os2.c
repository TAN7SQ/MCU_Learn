#include "cmsis_os2.h"
#include "../common/elab_export.h"
#include "../common/elab_common.h"
#include "../elib/elib_queue.h"


/* Kernel functions */
osStatus_t osKernelInitialize(void) { return 0; }
osStatus_t osKernelStart(void)      { return 0; }


/*裸机情况下无锁*/
/* Mutex functions */
osMutexId_t osMutexNew(const osMutexAttr_t *attr) {
    (void)attr;
    return (osMutexId_t)1;  // 裸机情况下无锁
}

osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout) {
    (void)mutex_id;
    (void)timeout;
    return osOK;
}

osStatus_t osMutexRelease(osMutexId_t mutex_id) {
    (void)mutex_id;
    return osOK;
}

osStatus_t osMutexDelete(osMutexId_t mutex_id) {
    (void)mutex_id;
    return osOK;
}

/*裸机情况下无信号量 */
/* Semaphore functions (空操作实现) */
osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr) {
    (void)max_count;
    (void)initial_count;
    (void)attr;
    return (osSemaphoreId_t)1;  // 返回非空指针表示创建成功
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout) {
    (void)semaphore_id;
    (void)timeout;
    return osOK;  // 始终返回成功
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id) {
    (void)semaphore_id;
    return osOK;  // 始终返回成功
}

osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id) {
    (void)semaphore_id;
    return osOK;  // 始终返回成功
}
/*裸机情况下无线程*/
/* Stub implementations for threads (not used in bare metal) */
osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
    (void)func; (void)argument; (void)attr;
    return NULL;
}


osThreadId_t osThreadGetId (void)
{
    (void)0;
    return NULL;
}

uint32_t osKernelGetTickCount (void)
{
return elab_time_ms();

}


/**裸机情况下定时器CMSISOS2 API */
#if ELAB_RTOS_CMSIS_OS_EN != 1
#define MAX_ELAB_NOOS_CMSIS_TIMERS 16 //最大16个定时任务
#endif

typedef struct {
    osTimerFunc_t callback;
    void *argument;
    uint32_t period_ms;
    bool is_periodic;
    bool is_running;
    uint32_t timeout_ms;
} elab_cmsis_timer_t;



static elab_cmsis_timer_t *g_cmsis_timers[MAX_ELAB_NOOS_CMSIS_TIMERS] = {0};

//no_os_timer

static void elab_cmsis_timer_poll(void)
{
    uint32_t now = elab_time_ms();

    for (int i = 0; i < MAX_ELAB_NOOS_CMSIS_TIMERS; i++) {
        elab_cmsis_timer_t *timer = g_cmsis_timers[i];
        if (timer && timer->is_running) {
            if ((int32_t)(now - timer->timeout_ms) >= 0) {
                timer->callback(timer->argument);
                if (timer->is_periodic) {
                    timer->timeout_ms += timer->period_ms;
                } else {
                    timer->is_running = false;
                }
            }
        }
    }
}

POLL_EXPORT(elab_cmsis_timer_poll, 1); //elab定时器轮询 1ms轮询定时器1次，定时器最小时基为1ms



osTimerId_t osTimerNew(osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr)
{
    (void)attr;
    for (int i = 0; i < MAX_ELAB_NOOS_CMSIS_TIMERS; i++) {
        if (g_cmsis_timers[i] == NULL) {
            elab_cmsis_timer_t *timer = malloc(sizeof(elab_cmsis_timer_t));
            if (!timer) return NULL;

            timer->callback = func;
            timer->argument = argument;
            timer->is_periodic = (type == osTimerPeriodic);
            timer->is_running = false;
            timer->period_ms = 0;
            timer->timeout_ms = 0;

            g_cmsis_timers[i] = timer;
            return (osTimerId_t)timer;
        }
    }
    return NULL;
}

osStatus_t osTimerStart(osTimerId_t timer_id, uint32_t ticks)
{
    if (!timer_id || ticks == 0) return osErrorParameter;
    elab_cmsis_timer_t *timer = (elab_cmsis_timer_t *)timer_id;

    timer->period_ms = ticks;
    timer->timeout_ms = elab_time_ms() + ticks;
    timer->is_running = true;

    return osOK;
}

osStatus_t osTimerStop(osTimerId_t timer_id)
{
    if (!timer_id) return osErrorParameter;
    elab_cmsis_timer_t *timer = (elab_cmsis_timer_t *)timer_id;

    timer->is_running = false;
    return osOK;
}



/***
 * 裸机情况下message queue 
 * @note 注意msg_size被截断为uint16_t类型 实际message_size最大值为 MAX(uint16_t)
 */


typedef struct {
    elib_queue_t queue; // eLab队列
    uint32_t msg_size; // 消息大小
    uint32_t capacity;//队列容量(消息数量)
} osMessageQueue_t;


/**
 * @brief Create and initialize a message queue.
 * @param msg_count Number of messages the queue can hold.
 * @param msg_size Size of each message in bytes.
 * @param attr Queue attributes (not used in this implementation).
 * @return Message queue ID or NULL in case of error.
 */
osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
    (void)attr; // 属性未使用

    // 分配消息队列结构体
    osMessageQueue_t *mq = malloc(sizeof(osMessageQueue_t));
    if (!mq) {
        return NULL;
    }

    // 分配队列缓冲区
    void *buffer = malloc(msg_count * msg_size);
    if (!buffer) {
        free(mq);
        return NULL;
    }

    // 初始化队列
    elib_queue_init(&mq->queue, buffer, msg_count * msg_size);

    // 设置消息大小和容量
    mq->msg_size = msg_size;
    mq->capacity = msg_count;

    return (osMessageQueueId_t)mq;
}

/**
 * @brief Put a message into a queue.
 * @param mq_id Message queue ID.
 * @param msg_ptr Pointer to the message.
 * @param msg_prio Message priority (not used in this implementation).
 * @param timeout Timeout value or 0 in case of no timeout.
 * @return Status code indicating the execution status of the function.
 */
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
    (void)msg_prio; // 优先级未使用

    osMessageQueue_t *mq = (osMessageQueue_t *)mq_id;
    if (!mq || !msg_ptr) {
        return osErrorParameter;
    }

    while (elib_queue_push(&mq->queue, (void *)msg_ptr, mq->msg_size) < 0) {
        (void)timeout;
        return osErrorTimeout;

    }

    return osOK;
}

/**
 * @brief Get a message from a queue.
 * @param mq_id Message queue ID.
 * @param msg_ptr Pointer to buffer for the message.
 * @param msg_prio Pointer to message priority (not used in this implementation).
 * @param timeout Timeout value or 0 in case of no timeout.
 * @return Status code indicating the execution status of the function.
 */
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
    (void)msg_prio; // 优先级未使用

    osMessageQueue_t *mq = (osMessageQueue_t *)mq_id;
    if (!mq || !msg_ptr) {
        return osErrorParameter;
    }


    while (elib_queue_pull_pop(&mq->queue, msg_ptr, mq->msg_size) == 0) {
        (void)timeout;
        return osErrorTimeout;

    }

    return osOK;
}

/**
 * @brief Delete a message queue.
 * @param mq_id Message queue ID.
 * @return Status code indicating the execution status of the function.
 */
osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id) {
    osMessageQueue_t *mq = (osMessageQueue_t *)mq_id;
    if (!mq) {
        return osErrorParameter;
    }

    // 释放队列缓冲区和消息队列结构体
    free(mq->queue.buffer);
    free(mq);
    return osOK;
}


