# ESP-IDF Blink Project - ESP32-C3 Supermini
cmake_minimum_required(VERSION 3.16)

# Generate wifi_config.h from .env trước khi build
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env python3 "${CMAKE_SOURCE_DIR}/scripts/gen_wifi_config.py"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blink)
