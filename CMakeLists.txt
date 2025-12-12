#   STM32 Minimal CMake project for C/C++ projects
cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/arm-none-eabi-gcc.cmake)
set(CMAKE_GENERATOR make)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
###############################################################################
# Set project name and source code folder location
project(GD32F103C8T6)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
option(DUMP_ASM "Create full assembly of final executable" OFF)
# Set microcontroller information
set(MCU_FAMILY GD32F1x0)
set(CPU_PARAMETERS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft)
set(STARTUP_SCRIPT ${PROJECT_DIR}/GD_libraries/CMSIS/DeviceSupport/startup_gd32f10x.s)
set(MCU_LINKER_SCRIPT ${PROJECT_DIR}/Utilities/GD32F130C8T6.ld)

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





add_executable(${EXECUTABLE}
    ${GD_STANDARD_LIB_SOURCE}

    ${CLOCK_SOURCE}

    

    ${PROJECT_DIR}/src/main.c
    )


target_compile_definitions(${EXECUTABLE} PRIVATE
    -D${MCU_FAMILY}
    -DUSE_STDPERIPH_DRIVER
    -DGD32F10X_MD
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
