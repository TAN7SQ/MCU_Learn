set(LIB_NAME "elab_libaray")

set(ELAB_SRC
    common/elab_log.c
    common/elab_common.c
    common/elab_assert.c
    common/elab_export.c
    3rd/list/list_iterator.c
    3rd/list/list_node.c
    3rd/list/list.c
    3rd/lwrb/lwrb.c
    3rd/ringbuf/ringbuf.c
    3rd/state_machine/state_machine.c
    3rd/Shell/shell_cmd_list.c
    3rd/Shell/shell_companion.c
    3rd/Shell/shell_ext.c
    3rd/Shell/shell.c
    elib/elib_crc.c
    elib/elib_queue.c
    elib/hash_table.c
    

)


set(XFUSION_SRC
    3rd/xfusion/xf_utils_log/xf_utils_log_dump.c
)

set(FREERTOS_SRC
    3rd/FreeRTOS/croutine.c
    3rd/FreeRTOS/event_groups.c
    3rd/FreeRTOS/list.c
    3rd/FreeRTOS/queue.c
    3rd/FreeRTOS/stream_buffer.c
    3rd/FreeRTOS/tasks.c
    3rd/FreeRTOS/timers.c
    3rd/FreeRTOS/portable/MemMang/heap_4.c
    
)

# if(WIN32) # WINDOWS  cmsis_os use freertos
# set(CMSIS_OS_SRC
#     ${FREERTOS_SRC}
#     os/win32/cmsis_os_cb.c
#     os/win32/cmsis_os.c
# )
# elseif(UNIX)
# set(CMSIS_OS_SRC
#     os/posix/cmsis_os.c
# )
# endif()


add_library(${LIB_NAME} STATIC "")

#将库文件输出到构建目录的Lib子目录
set_target_properties(${LIB_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# 链接 winmm 库 (仅在 Windows 上) for freertos
if(WIN32)
    set(CMSIS_OS_SRC
        ${FREERTOS_SRC}
        3rd/FreeRTOS/portable/mingw/port.c
        os/win32/cmsis_os_cb.c
        os/win32/cmsis_os.c
    )
    target_link_libraries(${LIB_NAME} PRIVATE winmm)
    target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/os/win32
    ${CMAKE_CURRENT_LIST_DIR}/os
    ${CMAKE_CURRENT_LIST_DIR}/3rd/FreeRTOS/include
    ${CMAKE_CURRENT_LIST_DIR}/3rd/FreeRTOS/portable/mingw
    )
elseif(UNIX)  #linux posix 接口
    set(CMSIS_OS_SRC
        os/posix/cmsis_os.c
    )
    target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/os/posix
    ${CMAKE_CURRENT_LIST_DIR}/os

    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic") #mcu平台
    if(DEFINED USE_OS AND USE_OS)  #使用freertos
        set(CMSIS_OS_SRC
        ${FREERTOS_SRC}
        3rd/FreeRTOS/portable/GCC/ARM_CM3/port.c
        os/mcu_os/cmsis_os2.c
        )
        target_include_directories(${LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/3rd/FreeRTOS/include
        ${CMAKE_CURRENT_LIST_DIR}/3rd/FreeRTOS/portable/GCC/ARM_CM3
        ${CMAKE_CURRENT_LIST_DIR}/os
        ${CMAKE_CURRENT_LIST_DIR}/os/mcu_os
        )
        # Link with stm32f1xx_libaray to get CMSIS headers automatically
        if(TARGET stm32f1xx_libaray)
            target_link_libraries(${LIB_NAME} PUBLIC stm32f1xx_libaray)
        endif()
    else()  #不使用操作系统
        set(CMSIS_OS_SRC
        os/mcu_noos/cmsis_os2.c
        )
        target_include_directories(${LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/os
        ${CMAKE_CURRENT_LIST_DIR}/os/mcu_noos
        )
    endif()

endif()

target_sources(${LIB_NAME} PUBLIC
    ${ELAB_SRC}
    ${XFUSION_SRC}
    ${CMSIS_OS_SRC}
)



target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/common
    ${CMAKE_CURRENT_LIST_DIR}/3rd/xfusion/xf_utils_log
    ${CMAKE_CURRENT_LIST_DIR}/3rd/xfusion/xf_common
    ${CMAKE_CURRENT_LIST_DIR}/3rd/xfusion
    ${CMAKE_CURRENT_LIST_DIR}/3rd/lwrb
    ${CMAKE_CURRENT_LIST_DIR}/3rd/list
    ${CMAKE_CURRENT_LIST_DIR}/3rd/ringbuf
    ${CMAKE_CURRENT_LIST_DIR}/3rd/state_machine
    ${CMAKE_CURRENT_LIST_DIR}/3rd/Shell
    ${CMAKE_CURRENT_LIST_DIR}/elib


)





