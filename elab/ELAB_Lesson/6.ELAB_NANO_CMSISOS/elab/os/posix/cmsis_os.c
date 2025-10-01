
/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#if defined(__linux__)

/* includes ----------------------------------------------------------------- */
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <semaphore.h>
#include "../cmsis_os.h"

#define RTOS_TIMER_NUM_MAX                      (32)
#define RTOS_TIMER_VALUE_MIN                    (1)

static int get_pthread_priority(osPriority_t prio);
static void _thread_entry_timer(void *para);

/* -----------------------------------------------------------------------------
Data structure
----------------------------------------------------------------------------- */
static const osMutexAttr_t mutex_attr_queue =
{
    "mutex_queue", 
    osMutexRecursive | osMutexPrioInherit, 
    NULL,
    0U 
};

static const osMutexAttr_t mutex_attr_timer =
{
    "mutex_timer",
    osMutexRecursive | osMutexPrioInherit,
    NULL,
    0U 
};

static const osMutexAttr_t mutex_attr_event_flag =
{
    "mutex_event_flag",
    osMutexRecursive | osMutexPrioInherit,
    NULL,
    0U 
};

enum timer_state
{
    TIMER_STATE_IDLE = 0,
    TIMER_STATE_RUN,
    TIMER_STATE_UNUSED,
};

typedef struct os_timer_data
{
    osTimerFunc_t func;
    osTimerType_t type;
    osTimerAttr_t attr;
    void *argument;
    uint32_t timeout;
    uint32_t ticks;
    uint8_t state;
} os_timer_data_t;

static os_timer_data_t timer[RTOS_TIMER_NUM_MAX];
static uint32_t timeout_min = 0;

static osMutexId_t mutex_timer = NULL;

/* -----------------------------------------------------------------------------
OS Basic
----------------------------------------------------------------------------- */
static const osThreadAttr_t thread_attr_timer =
{
    .name = "timer",
    .stack_size = 2048,
    .priority = (osPriority_t)osPriorityHigh7,
};

osStatus_t osKernelInitialize(void)
{
    for (uint32_t i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
    {
        timer[i].state = TIMER_STATE_UNUSED;
    }

    /* Create one thread for mutex function. */
    mutex_timer = osMutexNew(&mutex_attr_timer);
    assert(mutex_timer != NULL);

    /* Create one thread for timer function. */
    osThreadId_t thread = osThreadNew(_thread_entry_timer, NULL, NULL);
    assert(thread != NULL);

    return osOK;
}

osStatus_t osDelay(uint32_t ticks)
{
    struct timeval ts;
    ts.tv_usec = (ticks % 1000) * 1000;
    ts.tv_sec = ticks / 1000;
    select(0, NULL, NULL, NULL, &ts);

    return osOK;
}

osStatus_t osDelayMs (uint32_t ms)
{
    struct timeval ts;
    ts.tv_usec = (ms % 1000) * 1000;
    ts.tv_sec = ms / 1000;
    select(0, NULL, NULL, NULL, &ts);

    return osOK;
}

osStatus_t osDelayUs (uint32_t us)
{
    struct timeval ts;
    ts.tv_usec = us % 1000000;
    ts.tv_sec = us / 1000000;
    select(0, NULL, NULL, NULL, &ts);

    return osOK;
}

int32_t osKernelLock(void)
{
    /* 1 - locked, 0 - not locked, error code if negative. */
    return 1;   
}

int32_t osKernelUnlock(void)
{
    /* 1 - locked, 0 - not locked, error code if negative. */
    return 0;
}

static uint64_t time_init = UINT64_MAX;
uint32_t osKernelGetTickCount(void)
{
    /* Get the current time. */
    struct timeval tt;
    gettimeofday(&tt, NULL);
    uint64_t time_current = (tt.tv_sec * 1000) + (tt.tv_usec / 1000);

    if (time_init == UINT64_MAX)
    {
        time_init = time_current;
    }

    return (uint32_t)(time_current - time_init);
}

uint32_t osKernelGetSysTimerCount(void)
{
    return osKernelGetTickCount();
}

int16_t elab_debug_uart_receive(void *buffer, uint16_t size);

osStatus_t osKernelStart(void)
{
    while (true)
    {
        osDelay(1000);
    }

    return osOK;
}

/* -----------------------------------------------------------------------------
Thread
----------------------------------------------------------------------------- */
typedef void *(*os_pthread_func_t) (void *argument);

osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr)
{
    int ret = 0;
    pthread_t thread = 0;
    pthread_attr_t thread_attr;
    struct sched_param param;

    ret = pthread_attr_init(&thread_attr);
    assert(ret == 0);

    ret = pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
    assert(ret == 0);

    ret = pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
    assert(ret == 0);

    uint32_t stack_size = 40960;
    if (attr != NULL)
    {
        param.sched_priority = get_pthread_priority(attr->priority);
        stack_size = attr->stack_size * 20;
    }
    else
    {
        param.sched_priority = get_pthread_priority(osPriorityNormal);
    }
    ret = pthread_attr_setstacksize(&thread_attr, stack_size);
    assert(ret == 0);
    ret = pthread_attr_setschedparam(&thread_attr, &param);
    assert(ret == 0);
    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    assert(ret == 0);
    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    assert(ret == 0);
    ret = pthread_create(&thread, &thread_attr, (os_pthread_func_t)func, argument);
    assert(ret == 0);

    pthread_attr_destroy(&thread_attr);

    return (osThreadId_t)thread;
}

osThreadId_t osThreadGetId(void)
{
    return (osThreadId_t)pthread_self();
}

osStatus_t osThreadTerminate(osThreadId_t thread_id)
{
    int ret = 0;
    for (uint32_t i = 0; i < 100; i ++)
    {
        pthread_cancel((pthread_t)thread_id);
        ret = pthread_kill((pthread_t)thread_id, 0);
        if (ret != 0)
        {
            goto exit;
        }
        osDelay(1);
    }
    printf("pthread_kill ret: %d.\n", ret);
    assert(false);

exit:
    return osOK;
}

void osThreadExit(void)
{
    pthread_exit(NULL);
}

osStatus_t osThreadJoin(osThreadId_t thread_id)
{
    assert(thread_id != NULL);

    int ret = pthread_join((pthread_t)thread_id, NULL);
    assert(ret == 0);

    return osOK;
}

/* -----------------------------------------------------------------------------
Message queue
----------------------------------------------------------------------------- */
typedef struct os_mq
{
    osMutexId_t mutex;
    osSemaphoreId_t sem_empty;
    osSemaphoreId_t sem_full;
    uint8_t *memory;
    uint32_t msg_size;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    bool empty;
    bool full;
} os_mq_t;

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count,
                                     uint32_t msg_size,
                                     const osMessageQueueAttr_t *attr)
{
    assert(attr == NULL);
    assert(msg_count != 0);
    assert(msg_size != 0);

    os_mq_t *mq = malloc(sizeof(os_mq_t));
    assert(mq != NULL);

    mq->mutex = osMutexNew(&mutex_attr_queue);
    assert(mq->mutex != NULL);
    mq->sem_full = osSemaphoreNew(msg_count, msg_count, NULL);
    assert(mq->sem_full != NULL);
    mq->sem_empty = osSemaphoreNew(msg_count, 0, NULL);
    assert(mq->sem_empty != NULL);

    mq->capacity = msg_count;
    mq->msg_size = msg_size;
    mq->memory = (uint8_t *)malloc(msg_size * msg_count);
    assert(mq->memory != NULL);

    mq->head = 0;
    mq->tail = 0;
    mq->empty = true;
    mq->full = false;

    return (osMessageQueueId_t)mq;
}

osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id)
{
    os_mq_t *mq = (os_mq_t *)mq_id;

    osMutexDelete(mq->mutex);
    free(mq->memory);
    free(mq);

    return osOK;
}

osStatus_t osMessageQueueReset(osMessageQueueId_t mq_id)
{
    os_mq_t *mq = (os_mq_t *)mq_id;

    osMutexAcquire(mq->mutex, osWaitForever);

    mq->head = 0;
    mq->tail = 0;
    mq->empty = true;
    mq->full = false;

    osMutexRelease(mq->mutex);

    return osOK;
}

osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id,
                             const void *msg_ptr,
                             uint8_t msg_prio,
                             uint32_t timeout)
{
    (void)msg_prio;

    osStatus_t ret_os = osOK;

    if (timeout != osWaitForever)
    {
        assert(timeout < (1000 * 60 * 60 * 24));
    }

    os_mq_t *mq = (os_mq_t *)mq_id;
    ret_os = osSemaphoreAcquire(mq->sem_full, timeout);
    assert(ret_os == osOK);

    ret_os = osMutexAcquire(mq->mutex, osWaitForever);
    assert(ret_os == osOK);
    assert(mq->full == false || mq->empty == false);
    memcpy(&mq->memory[mq->head * mq->msg_size], msg_ptr, mq->msg_size);
    mq->head = (mq->head + 1) % mq->capacity;
    mq->empty = false;
    if (mq->head == mq->tail)
    {
        mq->full = true;
    }
    ret_os = osMutexRelease(mq->mutex);
    assert(ret_os == osOK);
    ret_os = osSemaphoreRelease(mq->sem_empty);
    assert(ret_os == osOK);

    return osOK;
}

osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id,
                             void *msg_ptr,
                             uint8_t *msg_prio,
                             uint32_t timeout)
{
    (void)msg_prio;

    osStatus_t ret_os = osOK;

    if (timeout != osWaitForever)
    {
        assert(timeout < (1000 * 60 * 60 * 24));
    }

    os_mq_t *mq = (os_mq_t *)mq_id;
    osStatus_t ret = osSemaphoreAcquire(mq->sem_empty, timeout);
    if (ret == osErrorTimeout)
    {
        return osErrorTimeout;
    }

    ret_os = osMutexAcquire(mq->mutex, osWaitForever);
    assert(ret_os == osOK);
    assert(mq->full == false || mq->empty == false);
    memcpy(msg_ptr, &mq->memory[mq->tail * mq->msg_size], mq->msg_size);
    mq->tail = (mq->tail + 1) % mq->capacity;
    mq->full = false;
    if (mq->head == mq->tail)
    {
        mq->empty = true;
    }
    ret_os = osMutexRelease(mq->mutex);
    assert(ret_os == osOK);
    ret_os = osSemaphoreRelease(mq->sem_full);
    assert(ret_os == osOK);

    return osOK;
}

/* -----------------------------------------------------------------------------
Mutex
----------------------------------------------------------------------------- */
typedef struct os_mutex_data
{
    osMutexAttr_t attr;
    pthread_mutex_t mutex;
} os_mutex_data_t;

osMutexId_t osMutexNew(const osMutexAttr_t *attr)
{
    (void)attr;

    os_mutex_data_t *data = malloc(sizeof(os_mutex_data_t));
    assert(data != NULL);

    int ret = pthread_mutex_init(&data->mutex, NULL);
    assert(ret == 0);

    memcpy(&data->attr, attr, sizeof(osMutexAttr_t));

    return (osMutexId_t)data;
}

osStatus_t osMutexDelete(osMutexId_t mutex_id)
{
    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;

    int ret = pthread_mutex_destroy(&data->mutex);
    assert(ret == 0);

    free(data);

    return osOK;
}

osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout)
{
    assert(mutex_id != NULL);

    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;
    assert(timeout == osWaitForever);

    int ret = pthread_mutex_lock(&data->mutex);
    if (ret != 0)
    {
        return osError;
    }
    else
    {
        return osOK;
    }
}

osStatus_t osMutexRelease(osMutexId_t mutex_id)
{
    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;

    int ret = pthread_mutex_unlock(&data->mutex);
    if (ret != 0)
    {
        return osError;
    }
    else
    {
        return osOK;
    }
}

/* -----------------------------------------------------------------------------
Semaphore
----------------------------------------------------------------------------- */
#include <semaphore.h>

osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr)
{
    assert(initial_count <= max_count);

    sem_t *sem = malloc(sizeof(sem_t));
    assert(sem != NULL);

    int ret = sem_init(sem, 0, max_count);
    assert(ret == 0);

    for (uint32_t i = initial_count; i < max_count; i ++)
    {
        ret = sem_wait(sem);
        assert(ret == 0);
    }

    return (osSemaphoreId_t)sem;
}

osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id)
{
    sem_destroy((sem_t *)semaphore_id);
    free((sem_t *)semaphore_id);

    return osOK;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id)
{
    int ret = sem_post((sem_t *)semaphore_id);
    assert(ret == 0);

    return osOK;
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout)
{
    assert(semaphore_id != NULL);

    int ret;
    if (timeout == osWaitForever)
    {
        ret = sem_wait((sem_t *)semaphore_id);
        assert(ret == 0);
    }
    else if (timeout == 0)
    {
        ret = sem_trywait((sem_t *)semaphore_id);
        if (ret != 0)
        {
            return osErrorResource;
        }
    }
    else
    {
        struct timeval tt;
        gettimeofday(&tt, NULL);

        uint32_t second = timeout / 1000;
        uint32_t ns = (timeout % 1000) * 1000 * 1000;

        struct timespec ts;
        ts.tv_sec = tt.tv_sec + second;
        ts.tv_nsec = tt.tv_usec * 1000 + ns;
        ts.tv_sec += (ts.tv_nsec / (1000 * 1000 * 1000));
        ts.tv_nsec %= (1000 * 1000 * 1000);

        ret = sem_timedwait((sem_t *)semaphore_id, &ts);
        if (ret == -1)
        {
            if (errno == ETIMEDOUT)
            {
                return osErrorTimeout;
            }
        }
    }

    return osOK;
}

/* -----------------------------------------------------------------------------
Timer
----------------------------------------------------------------------------- */
osTimerId_t osTimerNew(osTimerFunc_t func,
                        osTimerType_t type,
                        void *argument,
                        const osTimerAttr_t *attr)
{
    assert(func != NULL);
    assert(type == osTimerOnce || type == osTimerPeriodic);
    assert(attr != NULL);

    osStatus_t ret_os = osOK;

    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    uint32_t i = 0;
    for (i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
    {
        if (timer[i].state == (uint8_t)TIMER_STATE_UNUSED)
        {
            break;
        }
    }
    timer[i].func = func;
    timer[i].type = type;
    timer[i].argument = argument;
    timer[i].state = (uint8_t)TIMER_STATE_IDLE;
    memcpy(&timer[i].attr, attr, sizeof(osTimerAttr_t));

    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return (osTimerId_t)&timer[i];
}

const char *osTimerGetName (osTimerId_t timer_id)
{
    assert(timer_id != NULL);
    
    osStatus_t ret_os = osOK;
    os_timer_data_t *timer = (os_timer_data_t *)timer_id;
    const char *name;

    /* Lock the mutex. */
    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    name = timer->attr.name;

    /* Unlock the mutex. */
    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return name;
}

uint32_t osTimerIsRunning(osTimerId_t timer_id)
{
    assert(timer_id != NULL);
    
    osStatus_t ret_os = osOK;
    os_timer_data_t *timer = (os_timer_data_t *)timer_id;
    uint32_t ret = 0;

    /* Lock the mutex. */
    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    ret = timer->state == TIMER_STATE_RUN ? 1 : 0;

    /* Unlock the mutex. */
    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return ret;
}

osStatus_t osTimerStart(osTimerId_t timer_id, uint32_t ticks)
{
    assert(timer_id != NULL);
    assert(ticks >= RTOS_TIMER_VALUE_MIN);

    osStatus_t ret_os = osOK;
    os_timer_data_t *timer = (os_timer_data_t *)timer_id;

    /* Lock the mutex. */
    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    /* Start the timer. */
    timer->timeout = ticks + osKernelGetTickCount();
    timer->ticks = ticks;
    timer->state = TIMER_STATE_RUN;
    timeout_min = timeout_min > timer->timeout ? timer->timeout : timeout_min;

    /* Unlock the mutex. */
    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return osOK;
}

osStatus_t osTimerStop(osTimerId_t timer_id)
{
    assert(timer_id != NULL);

    osStatus_t ret_os = osOK;
    os_timer_data_t *timer = (os_timer_data_t *)timer_id;

    /* Lock the mutex. */
    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    timer->state = TIMER_STATE_IDLE;
    if (timeout_min <= timer->timeout &&
        timer->state == (uint8_t)TIMER_STATE_RUN)
    {
        timeout_min = UINT32_MAX;
        for (uint32_t i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
        {
            if (timer[i].state == (uint8_t)TIMER_STATE_RUN &&
                timeout_min > timer[i].timeout)
            {
                timeout_min = timer[i].timeout;
            }
        }
    }

    /* Unlock the mutex. */
    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return osOK;
}

osStatus_t osTimerDelete(osTimerId_t timer_id)
{
    assert(timer_id != NULL);

    osStatus_t ret_os = osOK;
    os_timer_data_t *timer = (os_timer_data_t *)timer_id;

    /* Lock the mutex. */
    ret_os = osMutexAcquire(mutex_timer, osWaitForever);
    assert(ret_os == osOK);

    timer->state = TIMER_STATE_UNUSED;
    if (timeout_min <= timer->timeout &&
        timer->state == (uint8_t)TIMER_STATE_RUN)
    {
        timeout_min = UINT32_MAX;
        for (uint32_t i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
        {
            if (timer[i].state == (uint8_t)TIMER_STATE_RUN &&
                timeout_min > timer[i].timeout)
            {
                timeout_min = timer[i].timeout;
            }
        }
    }

    /* Unlock the mutex. */
    ret_os = osMutexRelease(mutex_timer);
    assert(ret_os == osOK);

    return osOK;
}

/* -----------------------------------------------------------------------------
Event Flag
----------------------------------------------------------------------------- */
typedef struct os_event_flags
{
    osSemaphoreId_t sem;
    osMutexId_t mutex;
    uint32_t flags;
    uint32_t flags_set;
    osEventFlagsAttr_t attr;
    uint32_t time_start;
} os_event_flags_t;

osEventFlagsId_t osEventFlagsNew(const osEventFlagsAttr_t *attr)
{
    os_event_flags_t *evt_flags = malloc(sizeof(os_event_flags_t));
    assert(evt_flags != NULL);

    evt_flags->sem = osSemaphoreNew(1, 0, NULL);
    assert(evt_flags->sem != NULL);
    evt_flags->mutex = osMutexNew(&mutex_attr_event_flag);
    assert(evt_flags->mutex != NULL);

    evt_flags->flags = 0;
    evt_flags->flags_set = 0;
    memcpy(&evt_flags->attr, attr, sizeof(osEventFlagsAttr_t));

    return (osEventFlagsId_t)evt_flags;
}

const char *osEventFlagsGetName (osEventFlagsId_t ef_id)
{
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    return evt_flags->attr.name;
}

uint32_t osEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout)
{
    /* TODO osFlagsNoClear */

    assert((options & osFlagsNoClear) == 0);
    assert(ef_id != NULL);
    assert((flags & 0x80000000) == 0);

    osStatus_t ret_os = osOK;
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    evt_flags->time_start = osKernelGetTickCount();
    evt_flags->flags = flags;
    uint32_t ret = flags;
    uint32_t _timeout = timeout;

    while (1)
    {
        ret_os = osSemaphoreAcquire(evt_flags->sem, _timeout);
        if (ret_os == osErrorTimeout)
        {
            ret = (uint32_t)ret_os;
        }
        else if (ret_os == osOK)
        {
            ret = evt_flags->flags;
            evt_flags->flags &=~ evt_flags->flags_set;

            if (options & osFlagsWaitAll)
            {
                if (evt_flags->flags != 0)
                {
                    if (timeout > 0U)
                    {
                        ret = (uint32_t)osErrorTimeout;
                    }
                    else
                    {
                        ret = (uint32_t)osErrorResource;
                    }

                    _timeout = timeout - (osKernelGetTickCount() - evt_flags->time_start);
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (evt_flags->flags == flags)
                {
                    if (timeout > 0U)
                    {
                        ret = (uint32_t)osErrorTimeout;
                    }
                    else
                    {
                        ret = (uint32_t)osErrorResource;
                    }

                    _timeout = timeout - (osKernelGetTickCount() - evt_flags->time_start);
                }
                else
                {
                    break;
                }
            }
        }
    }

    return ret;
}

uint32_t osEventFlagsSet(osEventFlagsId_t ef_id, uint32_t flags)
{
    assert(ef_id != NULL);
    assert((flags & 0x80000000) == 0);

    osStatus_t ret_os = osOK;
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    evt_flags->flags_set = flags;
    ret_os = osSemaphoreRelease(evt_flags->sem);
    assert(ret_os == osOK);

    return evt_flags->flags_set;
}

uint32_t osEventFlagsGet (osEventFlagsId_t ef_id)
{
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    return evt_flags->flags_set;
}

uint32_t osEventFlagsClear(osEventFlagsId_t ef_id, uint32_t flags)
{
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    uint32_t ret = evt_flags->flags_set;
    evt_flags->flags_set &=~ flags;

    return ret;
}

osStatus_t osEventFlagsDelete (osEventFlagsId_t ef_id)
{
    osStatus_t ret_os = osOK;
    os_event_flags_t *evt_flags = (os_event_flags_t *)ef_id;
    ret_os = osSemaphoreDelete(evt_flags->sem);
    assert(ret_os == osOK);

    free(evt_flags);

    return osOK;
}

/* private function --------------------------------------------------------- */

static int get_pthread_priority(osPriority_t prio)
{
    if (prio > osPriorityRealtime7)
    {
        prio = osPriorityRealtime7;
    }

    return (osPriorityISR + 8 - (osPriorityRealtime7 - (prio)));
}

static void _thread_entry_timer(void *para)
{
    osStatus_t ret_os = osOK;

    while (1)
    {
        ret_os = osMutexAcquire(mutex_timer, osWaitForever);
        assert(ret_os == osOK);

        if (timeout_min <= osKernelGetTickCount())
        {
            for (uint32_t i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
            {
                if (timer[i].state != TIMER_STATE_RUN)
                {
                    continue;
                }

                if (osKernelGetTickCount() >= timer[i].timeout)
                {
                    timer[i].func(timer[i].argument);
                    if (timer[i].type == osTimerPeriodic)
                    {
                        timer[i].timeout += timer[i].ticks;
                    }
                    else
                    {
                        timer[i].state = TIMER_STATE_IDLE;
                    }
                }
            }

            /* Update the timeout_min. */
            timeout_min = UINT32_MAX;
            for (uint32_t i = 0; i < RTOS_TIMER_NUM_MAX; i ++)
            {
                if (timeout_min > timer[i].timeout &&
                    timer[i].state == TIMER_STATE_RUN)
                {
                    timeout_min = timer[i].timeout;
                }
            }
        }
        
        ret_os = osMutexRelease(mutex_timer);
        assert(ret_os == osOK);

        osDelay(RTOS_TIMER_VALUE_MIN);
    }
}

#endif

/* ----------------------------- end of file -------------------------------- */
