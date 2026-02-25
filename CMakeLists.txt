# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(M2IDI_CV16 CXX C ASM)

set(PICO_BOARD board_m2idi_cv16)
set(PICO_BOARD_HEADER_DIRS ${PROJECT_SOURCE_DIR}/src)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# rest of your project
file(GLOB src_list CONFIGURE_DEPENDS "src/*.cpp")
add_executable(M2IDI_CV16
    ${src_list}
)
pico_generate_pio_header(M2IDI_CV16 ${CMAKE_CURRENT_LIST_DIR}/src/dac_spi.pio)

add_subdirectory(lib)

# Add library files
target_include_directories(M2IDI_CV16 PUBLIC src)

target_link_options(M2IDI_CV16 PUBLIC -Xlinker --print-memory-usage)
target_compile_options(M2IDI_CV16 PRIVATE -Wall -Wextra)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(M2IDI_CV16 pico_stdlib hardware_timer hardware_pwm hardware_spi hardware_dma hardware_pio pico_multicore libmidi2 mcp2517_driver max5134_driver eeprom_cat_driver)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(M2IDI_CV16)
