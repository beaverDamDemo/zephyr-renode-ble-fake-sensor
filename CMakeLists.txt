cmake_minimum_required(VERSION 3.20.0)

if(NOT DEFINED BOARD)
	set(BOARD native_sim)
endif()

get_filename_component(APP_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
get_filename_component(WORKSPACE_DIR ${APP_DIR} DIRECTORY)

if(NOT DEFINED ENV{ZEPHYR_BASE})
	set(ENV{ZEPHYR_BASE} ${WORKSPACE_DIR}/zephyr)
endif()

if(EXISTS ${WORKSPACE_DIR}/.venv/bin/python AND NOT DEFINED Python3_EXECUTABLE)
	set(Python3_EXECUTABLE ${WORKSPACE_DIR}/.venv/bin/python CACHE FILEPATH "Python 3 interpreter")
endif()

if(EXISTS ${WORKSPACE_DIR}/.zephyr-sdk-1.0.1/cmake/Zephyr-sdkConfig.cmake)
	set(Zephyr-sdk_DIR ${WORKSPACE_DIR}/.zephyr-sdk-1.0.1/cmake CACHE PATH "Path to Zephyr SDK CMake package")
	set(ZEPHYR_SDK_INSTALL_DIR ${WORKSPACE_DIR}/.zephyr-sdk-1.0.1 CACHE PATH "Path to Zephyr SDK install")
endif()

if(EXISTS /usr/bin/dtc)
	set(DTC /usr/bin/dtc CACHE FILEPATH "Path to devicetree compiler")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_ble_app)

target_sources(app PRIVATE src/main.c)
