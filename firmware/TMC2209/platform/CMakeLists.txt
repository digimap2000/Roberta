# Include interface headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/interface)

if(PLATFORM STREQUAL "stm32")
    set(PLATFORM_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/stm32/startup/startup_stm32f0xx.s
#        ${CMAKE_CURRENT_SOURCE_DIR}/stm32/drivers/uart_stm32.c
        PARENT_SCOPE
    )    
elseif(PLATFORM STREQUAL "host")
    set(PLATFORM_SOURCES
#        ${CMAKE_CURRENT_SOURCE_DIR}/host/drivers/uart_host.c
        PARENT_SCOPE
    )
endif()
