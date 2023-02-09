cmake_minimum_required(VERSION 3.13)

# initialize the SDK directly
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(portarig)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB SRC_FILES
    src/*.c
    src/**/*.c
    )

list(FILTER SRC_FILES EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/src/bin/")

add_executable(client ${SRC_FILES} src/bin/client.c)

target_include_directories(client PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(client PRIVATE
  pico_stdlib
  pico_stdio
  tinyusb_device
  tinyusb_board
  hardware_pio
  hardware_irq
  hardware_dma
  )

pico_enable_stdio_usb(client 0)
pico_enable_stdio_uart(client 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(client)
