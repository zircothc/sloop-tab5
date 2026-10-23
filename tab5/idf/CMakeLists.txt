# SPDX-License-Identifier: GPL-3.0-only
# SLOOP (M-VAVE FM-1 firmware) on the M5Stack Tab5 (ESP32-P4).
#   idf.py build                         the real Tab5 (pulls espressif/m5stack_tab5_noglib from the component registry)
#   idf.py -DSLOOP_BOARD=null build      headless: compiles and sizes the firmware without board drivers (works offline)
cmake_minimum_required(VERSION 3.16)
set(SLOOP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "checkout of isod89/sloop-fm1 this tab5/ directory lives in")
set(SLOOP_BOARD "bsp" CACHE STRING "board layer: bsp (the Tab5) | null (headless)")
find_program(SLOOP_PYTHON NAMES python python3 REQUIRED)
execute_process(COMMAND "${SLOOP_PYTHON}" "${SLOOP_ROOT}/tab5/gen_headers.py" RESULT_VARIABLE gen_rc ERROR_VARIABLE gen_err)
if(NOT gen_rc EQUAL 0)
    message(FATAL_ERROR "tab5/gen_headers.py failed:\n${gen_err}")
endif()
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components" "${CMAKE_CURRENT_LIST_DIR}/boards/${SLOOP_BOARD}")
# this firmware is for the ESP32-P4 only: do not let a missing .vscode setting fall back to plain "esp32"
set(ENV{IDF_TARGET} esp32p4)
set(IDF_TARGET esp32p4 CACHE STRING "target chip" FORCE)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sloop_tab5)
