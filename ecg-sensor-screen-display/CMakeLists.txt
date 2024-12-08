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
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ecg-sensor-screen-display C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_subdirectory(lib/Config)
add_subdirectory(lib/LCD)
add_subdirectory(lib/GUI)
add_subdirectory(lib/Fonts)
add_subdirectory(examples)

#添加头文件目录
include_directories(examples)
include_directories(./lib/Config)
include_directories(./lib/LCD)
include_directories(./lib/GUI)

add_executable(ecg-sensor-screen-display ecg-sensor-screen-display.cpp )

pico_set_program_name(ecg-sensor-screen-display "ecg-sensor-screen-display")
pico_set_program_version(ecg-sensor-screen-display "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ecg-sensor-screen-display 0)
pico_enable_stdio_usb(ecg-sensor-screen-display 1)

# Add the standard library to the build
target_link_libraries(ecg-sensor-screen-display
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(ecg-sensor-screen-display PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(ecg-sensor-screen-display 
        hardware_spi
        hardware_adc
		hardware_dma
        pico_cyw43_arch_none
        examples
        GUI
        Fonts
        Config
        LCD
        )

pico_add_extra_outputs(ecg-sensor-screen-display)

