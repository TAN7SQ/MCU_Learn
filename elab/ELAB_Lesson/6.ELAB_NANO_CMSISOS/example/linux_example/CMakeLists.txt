cmake_minimum_required(VERSION 3.20)
set(__PROJ_NAME__ "Linux_example" CACHE STRING "Proj name")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${__PROJ_NAME__} C CXX ASM)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()



add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../elab ${CMAKE_CURRENT_BINARY_DIR}/elab_libaray)

set(LINUX_DEMO_SRC
    linux_example.c
)

add_executable(${__PROJ_NAME__})

target_sources(${__PROJ_NAME__} PRIVATE ${LINUX_DEMO_SRC})
target_link_libraries(${__PROJ_NAME__} PRIVATE "elab_libaray")
target_include_directories(elab_libaray PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)


target_include_directories(${__PROJ_NAME__} PRIVATE

)

message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
