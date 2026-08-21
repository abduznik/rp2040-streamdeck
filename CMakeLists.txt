cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(streamdeck C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(streamdeck
    src/main.c
    src/usb_descriptors.c
    src/mapping_store.c
    src/protocol.c
)

target_include_directories(streamdeck PRIVATE src)

# TinyUSB (bundled with Pico SDK) + hardware libs
target_link_libraries(streamdeck PRIVATE
    pico_stdlib
    pico_unique_id
    tinyusb_device
    tinyusb_board
    hardware_flash
    hardware_sync
)

pico_add_extra_outputs(streamdeck)

# We drive USB manually via TinyUSB, disable Pico SDK's default stdio-over-usb
pico_enable_stdio_usb(streamdeck 0)
pico_enable_stdio_uart(streamdeck 0)
