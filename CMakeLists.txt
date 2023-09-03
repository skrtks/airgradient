cmake_minimum_required(VERSION 3.25)
project(airgradient)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(examples)
include_directories(examples/C02_CALIBRATION)
include_directories(examples/C02_SIMPLE)
include_directories(examples/CO2_TRAFFIC_LIGHT)
include_directories(examples/DIY_BASIC)
include_directories(examples/DIY_OUTDOOR)
include_directories(examples/DIY_OUTDOOR_C3)
include_directories(examples/DIY_PRO)
include_directories(examples/DIY_PRO_V3_7)
include_directories(examples/DIY_PRO_V4_2)
include_directories(examples/MINI_DISPLAY)
include_directories(examples/PM2_SIMPLE)
include_directories(examples/SHT_SIMPLE)


# Root CMakeLists.txt

# Specify the directory containing your libraries
set(LIBRARY_DIR "/Users/samkortekaas/Documents/Arduino/libraries")

# List all subdirectories in the specified directory
file(GLOB SUBDIRS RELATIVE ${LIBRARY_DIR} ${LIBRARY_DIR}/*)

foreach(subdir ${SUBDIRS})
    if(IS_DIRECTORY ${LIBRARY_DIR}/${subdir})
        include_directories(${LIBRARY_DIR}/${subdir})
    endif()
endforeach()


add_executable(airgradient
        examples/C02_CALIBRATION/C02_CALIBRATION.ino
        examples/C02_SIMPLE/C02_SIMPLE.ino
        examples/CO2_TRAFFIC_LIGHT/CO2_TRAFFIC_LIGHT_SIMPLE.ino
        examples/DIY_BASIC/DIY_BASIC.ino
        examples/DIY_OUTDOOR/DIY_OUTDOOR.ino
        examples/DIY_OUTDOOR_C3/DIY_OUTDOOR_C3.ino
        examples/DIY_PRO/DIY_PRO.ino
        examples/DIY_PRO_V3_7/DIY_PRO_V3_7.ino
        examples/DIY_PRO_V4_2/DIY_PRO_V4_2.ino
        examples/MINI_DISPLAY/MINI_DISPLAY.ino
        examples/PM2_SIMPLE/PM2_SIMPLE.ino
        examples/SHT_SIMPLE/SHT_SIMPLE.ino
        AirGradient.cpp
        AirGradient.h)
