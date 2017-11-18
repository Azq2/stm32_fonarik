cmake_minimum_required(VERSION 2.8)

set (SOURCES
	src/main.c
	src/onewire.c
	src/printf.c
)
set (HEADERS
	src/main.h
	src/onewire.h
	src/printf.h
)

set (LIBS
	
)

set (CM3_PATH			"../CM3")
set (STD_PERIPH_PATH	"../STM32F10x_StdPeriph_Driver")
set (DEFS				"-DUSE_STDPERIPH_DRIVER -DSTM32F10X_MD")

###################################################################
project (test C ASM) 
INCLUDE(CMakeForceCompiler)
SET(CPREF				"arm-none-eabi")
SET(CMAKE_C_COMPILER	"${CPREF}-gcc")
SET(CMAKE_CXX_COMPILER	"${CPREF}-g++")
SET(CMAKE_ASM_COMPILER	"${CPREF}-gcc")
SET(CMAKE_AR			"${CPREF}-ar")
SET(CMAKE_LD			"${CPREF}-ld")
SET(CMAKE_OBJCOPY		"${CPREF}-objcopy")

CMAKE_FORCE_C_COMPILER(${CPREF}-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(${CPREF}-g++ GNU)

SET(STM32_LDFLAGS	"-mcpu=cortex-m3 -mthumb -mlittle-endian -mthumb-interwork -T linker/stm32_flash.ld -nostdlib")
SET(STM32_CFLAGS	"-mcpu=cortex-m3 -mthumb -mlittle-endian -mthumb-interwork -O3 -Os  -I . -I ./  ${DEFS} -s -fvisibility=hidden -fdata-sections -ffunction-sections -std=c99 -Wl,--gc-sections")

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STM32_CFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STM32_CFLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STM32_LDFLAGS}")

include_directories(inc src ${STD_PERIPH_PATH}/inc ${CM3_PATH}/CoreSupport ${CM3_PATH}/DeviceSupport/ST/STM32F10x)

SET(LIB_SRC
	# CMSIS
	${CM3_PATH}/DeviceSupport/ST/STM32F10x/startup/gcc_ride7/startup_stm32f10x_md.s
	${CM3_PATH}/DeviceSupport/ST/STM32F10x/system_stm32f10x.c
	
	# STM32F10x_StdPeriph_Driver
	${STD_PERIPH_PATH}/src/misc.c
	${STD_PERIPH_PATH}/src/stm32f10x_usart.c
	${STD_PERIPH_PATH}/src/stm32f10x_iwdg.c
	${STD_PERIPH_PATH}/src/stm32f10x_exti.c
	${STD_PERIPH_PATH}/src/stm32f10x_i2c.c
	${STD_PERIPH_PATH}/src/stm32f10x_adc.c
	${STD_PERIPH_PATH}/src/stm32f10x_flash.c
	${STD_PERIPH_PATH}/src/stm32f10x_rtc.c
	${STD_PERIPH_PATH}/src/stm32f10x_crc.c
	${STD_PERIPH_PATH}/src/stm32f10x_spi.c
	${STD_PERIPH_PATH}/src/stm32f10x_can.c
	${STD_PERIPH_PATH}/src/stm32f10x_pwr.c
	${STD_PERIPH_PATH}/src/stm32f10x_fsmc.c
	${STD_PERIPH_PATH}/src/stm32f10x_wwdg.c
	${STD_PERIPH_PATH}/src/stm32f10x_dma.c
	${STD_PERIPH_PATH}/src/stm32f10x_bkp.c
	${STD_PERIPH_PATH}/src/stm32f10x_gpio.c
	${STD_PERIPH_PATH}/src/stm32f10x_tim.c
	${STD_PERIPH_PATH}/src/stm32f10x_rcc.c
	${STD_PERIPH_PATH}/src/stm32f10x_dbgmcu.c
	${STD_PERIPH_PATH}/src/stm32f10x_sdio.c
	${STD_PERIPH_PATH}/src/stm32f10x_dac.c
	${STD_PERIPH_PATH}/src/stm32f10x_cec.c
)

SET(LIB_HEADERS
	# CMSIS
	${CM3_PATH}/DeviceSupport/ST/STM32F10x/system_stm32f10x.h
	
	# STM32F10x_StdPeriph_Driver
	${STD_PERIPH_PATH}/inc/misc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_gpio.h
	${STD_PERIPH_PATH}/inc/stm32f10x_rcc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_crc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_sdio.h
	${STD_PERIPH_PATH}/inc/stm32f10x_dbgmcu.h
	${STD_PERIPH_PATH}/inc/stm32f10x_flash.h
	${STD_PERIPH_PATH}/inc/stm32f10x_iwdg.h
	${STD_PERIPH_PATH}/inc/stm32f10x_dac.h
	${STD_PERIPH_PATH}/inc/stm32f10x_exti.h
	${STD_PERIPH_PATH}/inc/stm32f10x_bkp.h
	${STD_PERIPH_PATH}/inc/stm32f10x_wwdg.h
	${STD_PERIPH_PATH}/inc/stm32f10x_dma.h
	${STD_PERIPH_PATH}/inc/stm32f10x_adc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_cec.h
	${STD_PERIPH_PATH}/inc/stm32f10x_rtc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_i2c.h
	${STD_PERIPH_PATH}/inc/stm32f10x_can.h
	${STD_PERIPH_PATH}/inc/stm32f10x_pwr.h
	${STD_PERIPH_PATH}/inc/stm32f10x_tim.h
	${STD_PERIPH_PATH}/inc/stm32f10x_usart.h
	${STD_PERIPH_PATH}/inc/stm32f10x_fsmc.h
	${STD_PERIPH_PATH}/inc/stm32f10x_spi.h
)

add_executable (app.elf ${SOURCES} ${LIB_SRC} ${HEADERS} ${LIB_HEADERS})
target_link_libraries (app.elf)
add_custom_command (
	TARGET app.elf POST_BUILD     
	COMMAND ${CMAKE_OBJCOPY} -O binary app.elf app.bin
)
