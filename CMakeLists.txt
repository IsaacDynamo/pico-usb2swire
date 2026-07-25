# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-usb2swire C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico-usb2swire
    src/main.c
    src/swire.c
    src/swire_encode.c
    src/swire_flash.c
    src/crc16.c
    src/cdc_link.c
    src/usb_descriptors.c
)

pico_generate_pio_header(pico-usb2swire ${CMAKE_CURRENT_LIST_DIR}/src/swire.pio)

target_include_directories(pico-usb2swire PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_options(pico-usb2swire PRIVATE -Wall -Wextra -Wno-unused-parameter)

pico_set_program_name(pico-usb2swire "pico-usb2swire")
pico_set_program_description(pico-usb2swire "Telink TLSR82xx SWire programmer")
pico_set_program_url(pico-usb2swire "https://github.com/substanc3-dev/pico-usb2swire")
pico_set_program_version(pico-usb2swire "1.0")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-usb2swire 0)
pico_enable_stdio_usb(pico-usb2swire 0)

# Add the standard library to the build
target_link_libraries(pico-usb2swire
    pico_stdlib
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_watchdog
    tinyusb_device
    tinyusb_board
)

# Add the standard include files to the build
target_include_directories(pico-usb2swire PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(pico-usb2swire)
