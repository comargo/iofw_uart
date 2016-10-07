project(iofw_uart)
set(iofw_uart_sources
        src/iofw_uart.c
)

set(iofw_uart_headers
        inc/iofw_uart.h
        inc/circ.h
)

add_library(iofw_uart STATIC ${iofw_uart_sources} ${iofw_uart_headers})
target_link_libraries(iofw_uart iofw stm32f4xx_hal)
target_include_directories(iofw_uart PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
