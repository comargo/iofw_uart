cmake_minimum_required(VERSION 2.8)
project(iofw_uart)
set(iofw_uart_sources
        src/iofw_uart.c
)

set(iofw_uart_headers
        inc/iofw_uart.h
        inc/circ.h
)

add_subdirectory(iofw EXCLUDE_FROM_ALL)
add_subdirectory(stm32f4xx EXCLUDE_FROM_ALL)
add_library(iofw_uart STATIC ${iofw_uart_sources} ${iofw_uart_headers})
target_link_libraries(iofw_uart iofw stm32f4xx_hal)
target_include_directories(iofw_uart PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
