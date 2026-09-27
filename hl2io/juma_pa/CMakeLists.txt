# Firmware for the Hermes Lite 2 IO board that drives a JUMA amplifier.
#
# Unlike the firmwares that live inside the HL2IOBoard project, this one sits
# in the JUMA repository, because it shares the status parser and the band
# table with the ESP32 firmware here. It therefore needs to be told where the
# board project is:
#
#   export PICO_SDK_PATH=/path/to/pico-sdk
#   mkdir build && cd build
#   cmake -DHL2IOBOARD=/path/to/HL2IOBoard ..
#   make
#
# Build n2adr_lib in the board project first - this links its libhl2ioboard.a.
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH)
	if(DEFINED ENV{PICO_SDK_PATH})
		set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
	else()
		message(FATAL_ERROR
			"PICO_SDK_PATH is not set. Either export it or pass "
			"-DPICO_SDK_PATH=/path/to/pico-sdk")
	endif()
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Where the HL2IOBoard project is checked out. The default is a clone sitting
# next to this repository.
set(HL2IOBOARD "${CMAKE_CURRENT_LIST_DIR}/../../../HL2IOBoard"
	CACHE PATH "Path to a checkout of jimahlstrom/HL2IOBoard")
# The shared JUMA sources - the parser and the band table of the ESP32 firmware.
set(JUMA_SRC "${CMAKE_CURRENT_LIST_DIR}/../../src"
	CACHE PATH "Path to the shared JUMA sources (src/ of this repository)")

if(NOT EXISTS "${HL2IOBOARD}/hl2ioboard.h")
	message(FATAL_ERROR
		"No hl2ioboard.h under HL2IOBOARD=${HL2IOBOARD}. Clone "
		"https://github.com/jimahlstrom/HL2IOBoard and pass "
		"-DHL2IOBOARD=/path/to/HL2IOBoard")
endif()
if(NOT EXISTS "${HL2IOBOARD}/n2adr_lib/build/libhl2ioboard.a")
	message(FATAL_ERROR
		"libhl2ioboard.a is missing. Build it first:\n"
		"  cd ${HL2IOBOARD}/n2adr_lib && mkdir -p build && cd build && cmake .. && make")
endif()
if(NOT EXISTS "${JUMA_SRC}/juma_status.cpp")
	message(FATAL_ERROR "No juma_status.cpp under JUMA_SRC=${JUMA_SRC}")
endif()

project(HL2IOBoard_juma_pa C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(main
	main.cpp
	${JUMA_SRC}/juma_status.cpp
	${JUMA_SRC}/bands.cpp)
target_include_directories(main PRIVATE ${HL2IOBOARD} ${JUMA_SRC})
target_compile_options(main PRIVATE -Wall -Wextra)

# cmake -DJUMA_DEBUG=ON .. builds the talking image: the traffic to and from the
# PA goes to the USB serial port, and a command typed there is passed on to the
# amplifier. Use it to commission the wiring, not for normal operation.
option(JUMA_DEBUG "Trace the PA traffic on the USB serial port" OFF)
if(JUMA_DEBUG)
	target_compile_definitions(main PRIVATE JUMA_DEBUG=1)
	message(STATUS "JUMA_DEBUG is ON - this image prints to USB")
endif()

# These two set what REG_JUMA_MODE holds at power-up. Both bits can also be
# written over I2C at any time; the options only decide the state a board comes
# up in, which is what matters when no software on the host knows the registers.
#
#   -DJUMA_HOLD_OPERATE=ON  put the PA back into OPERATE when it drops to
#                           STANDBY - three tries, 5 s apart, never while an
#                           alarm is latched.
#   -DJUMA_TELEMETRY=ON     print one status line per second on the USB port.
#   -DJUMA_PROXY=ON         the USB port becomes the PA's serial port: a program
#                           on the PC speaks the JUMA command set straight at it.
#                           Leave JUMA_DEBUG off for this - a trace in the stream
#                           is something the PA would never say.
option(JUMA_HOLD_OPERATE "Hold the PA in OPERATE from power-up" OFF)
option(JUMA_TELEMETRY    "Print a status line per second on USB from power-up" OFF)
option(JUMA_PROXY        "Make the USB port the PA's serial port from power-up" OFF)

set(JUMA_MODE_AT_BOOT 0)
if(JUMA_HOLD_OPERATE)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 2")
	message(STATUS "JUMA_HOLD_OPERATE is ON - the PA is held in OPERATE")
endif()
if(JUMA_TELEMETRY)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 4")
	message(STATUS "JUMA_TELEMETRY is ON - a status line per second on USB")
endif()
if(JUMA_PROXY)
	math(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 8")
	message(STATUS "JUMA_PROXY is ON - the USB port is the PA's serial port")
	if(JUMA_DEBUG)
		message(WARNING "JUMA_DEBUG with JUMA_PROXY puts trace lines in the proxy stream")
	endif()
endif()
target_compile_definitions(main PRIVATE JUMA_MODE_AT_BOOT=${JUMA_MODE_AT_BOOT})

# The USB write blocks while a host is attached but not reading, and the default
# is half a second - long enough to eat into the PA's 5 s remote timeout. 10 ms
# bounds it: a stalled reader loses output instead of costing the PA its mode.
target_compile_definitions(main PRIVATE PICO_STDIO_USB_STDOUT_TIMEOUT_US=10000)

# USB for the debug output, and nothing on the board's UART - that one talks to
# the amplifier.
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)
pico_add_extra_outputs(main)

target_link_libraries(main
	pico_stdlib
	hardware_i2c
	hardware_pwm
	hardware_uart
	hardware_adc
	pico_i2c_slave
	${HL2IOBOARD}/n2adr_lib/build/libhl2ioboard.a)
