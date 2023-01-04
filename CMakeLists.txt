cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# HOST
add_executable(host
        host/main.c
)
pico_enable_stdio_usb(host 0)
pico_enable_stdio_uart(host 1)

pico_add_extra_outputs(host)

target_include_directories(host PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/host)

target_link_libraries(host pico_stdlib tinyusb_host tinyusb_board)

# DEVICE
add_executable(device
        device/main.c
        device/usb_descriptors.c
)
pico_enable_stdio_usb(device 0)
pico_enable_stdio_uart(device 1)

pico_add_extra_outputs(device)

target_include_directories(device PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/device)

target_link_libraries(device pico_stdlib tinyusb_device tinyusb_board) # tinyusb_device tinyusb_board)