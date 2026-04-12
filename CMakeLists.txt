#   STM32 Minimal CMake project for C/C++ projects
cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/arm-none-eabi-gcc.cmake)
set(CMAKE_GENERATOR make)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
###############################################################################
# Set project name and source code folder location
project(GD32F103RCT6)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
option(DUMP_ASM "Create full assembly of final executable" OFF)
# Set microcontroller information
set(MCU_FAMILY GD32F1x0)
set(CPU_PARAMETERS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft)
set(STARTUP_SCRIPT ${PROJECT_DIR}/GD_libraries/CMSIS/DeviceSupport/startup_gd32f10x.s)
set(MCU_LINKER_SCRIPT ${PROJECT_DIR}/Utilities/GD32F103RCT6.ld)

###############################################################################
set(EXECUTABLE ${CMAKE_PROJECT_NAME})
message(STATUS "CMAKE_TOOLCHAIN_FILE is: ${CMAKE_TOOLCHAIN_FILE}")
enable_language(C CXX ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_UNITY_BUILD ON)
###############################################################################
set(PROJECT_INCLUDE_DIRECTORIES
    ${PROJECT_DIR}/GD_libraries/CMSIS/CoreSupport/
    ${PROJECT_DIR}/GD_libraries/CMSIS/DeviceSupport/
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Include/
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/

    
    ${PROJECT_DIR}/src/
    ${PROJECT_DIR}/include/
    ${PROJECT_DIR}/include/clock/
    ${PROJECT_DIR}/include/led/
    ${PROJECT_DIR}/include/PWM/
    ${PROJECT_DIR}/include/DMA/
    ${PROJECT_DIR}/include/EXTI/
    ${PROJECT_DIR}/include/USB/
    ${PROJECT_DIR}/include/modbus/
    ${PROJECT_DIR}/include/lcd/

    # USB device library
    ${PROJECT_DIR}/GD_libraries/usbd/include/
    ${PROJECT_DIR}/GD_libraries/usbd/include/usbd_class/
    ${PROJECT_DIR}/GD_libraries/usbd/include/usbd_device/
    ${PROJECT_DIR}/GD_libraries/usbd/include/usbd_drivers/

    # FreeRTOS
    ${PROJECT_DIR}/FreeRTOS/include/
    ${PROJECT_DIR}/FreeRTOS/portable/GCC/ARM_CM3/
)

SET(GD_STANDARD_LIB_SOURCE
    ${PROJECT_DIR}/GD_libraries/CMSIS/DeviceSupport/startup_gd32f10x.s
    ${PROJECT_DIR}/GD_libraries/CMSIS/DeviceSupport/system_gd32f10x.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_adc.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_bkp.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_can.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_dac.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_dbg.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_dma.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_enet.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_exmc.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_exti.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_fmc.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_fwdgt.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_gpio.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_i2c.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_misc.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_pmu.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_rcu.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_rtc.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_sdio.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_spi.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_timer.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_usart.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_wwdgt.c
    ${PROJECT_DIR}/GD_libraries/GD32F10x_standard_peripheral/Source/gd32f10x_it.c
)


SET(CLOCK_SOURCE
    ${PROJECT_DIR}/src/clock/systick.c
)

SET(FREERTOS_SOURCE
    ${PROJECT_DIR}/FreeRTOS/tasks.c
    ${PROJECT_DIR}/FreeRTOS/queue.c
    ${PROJECT_DIR}/FreeRTOS/list.c
    ${PROJECT_DIR}/FreeRTOS/timers.c
    ${PROJECT_DIR}/FreeRTOS/event_groups.c
    ${PROJECT_DIR}/FreeRTOS/stream_buffer.c
    ${PROJECT_DIR}/FreeRTOS/portable/GCC/ARM_CM3/port.c
    ${PROJECT_DIR}/FreeRTOS/portable/MemMang/heap_4.c
)

# FreeRTOS cannot be compiled as Unity build (static symbol conflicts)
set_source_files_properties(${FREERTOS_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

# USB library has static symbols in multiple TUs – also opt out of Unity build
set(USBD_SOURCE
    ${PROJECT_DIR}/GD_libraries/usbd/src/gd32f10x_usbd_hw.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_class/cdc_acm_core.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_device/usbd_core.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_device/usbd_enum.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_device/usbd_pwr.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_device/usbd_transc.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_drivers/usbd_lld_core.c
    ${PROJECT_DIR}/GD_libraries/usbd/src/usbd_drivers/usbd_lld_int.c
)
set_source_files_properties(${USBD_SOURCE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)





add_executable(${EXECUTABLE}
    ${GD_STANDARD_LIB_SOURCE}
    ${FREERTOS_SOURCE}
    ${USBD_SOURCE}

    ${CLOCK_SOURCE}

    ${PROJECT_DIR}/src/clock/systick.c
    ${PROJECT_DIR}/src/led/led.c
    ${PROJECT_DIR}/src/clock/tim3_ms.c
    ${PROJECT_DIR}/src/PWM/PWM.c
    ${PROJECT_DIR}/src/DMA/dma.c
    ${PROJECT_DIR}/src/EXTI/EXTI.c
    ${PROJECT_DIR}/src/USB/usb_cdc.c
    ${PROJECT_DIR}/src/modbus/modbus.c
    ${PROJECT_DIR}/src/modbus/modbus_table.c
    ${PROJECT_DIR}/src/lcd/lcd_hd44780.c

    ${PROJECT_DIR}/src/main.c
    )


target_compile_definitions(${EXECUTABLE} PRIVATE
    -D${MCU_FAMILY}
    -DUSE_STDPERIPH_DRIVER
    -DGD32F10X_HD
     )
     
target_link_libraries(${EXECUTABLE} PRIVATE m)

target_include_directories(${EXECUTABLE} PRIVATE
    ${PROJECT_INCLUDE_DIRECTORIES})
###############################################################################
target_compile_options(${EXECUTABLE} PRIVATE
    ${CPU_PARAMETERS}
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
   # $<$<COMPILE_LANGUAGE:ASM>:
    #    -mcpu=cortex-m3
    $<$<COMPILE_LANGUAGE:CXX>:
        -Wconversion # STM libraries!
        -Wno-volatile
        -Wold-style-cast
        -Wuseless-cast
        -Wsuggest-override>
    $<$<CONFIG:Debug>:-O0 -g3 -ggdb>
    $<$<CONFIG:Release>:-Og -g0>)
    

target_link_options(${EXECUTABLE} PRIVATE
    -T${MCU_LINKER_SCRIPT}
    ${CPU_PARAMETERS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -mfloat-abi=soft
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments
    -Wl,--start-group
    -lc
    -lm
    -lstdc++
    -Wl,--end-group
    -Wl,--print-memory-usage
    -u _printf_float)

# The last command can take a couple of seconds on larger project, usefull for debugging
add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${EXECUTABLE}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${EXECUTABLE}> ${EXECUTABLE}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${EXECUTABLE}> ${EXECUTABLE}.bin)

    if (${DUMP_ASM})  
        add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -D $<TARGET_FILE:${EXECUTABLE}> > ${EXECUTABLE}.s)
    endif()
