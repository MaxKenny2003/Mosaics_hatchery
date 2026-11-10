cmake_minimum_required(VERSION 3.28.0)

# Tell Cmake we have made our own custom .overlay file
# Apparently we don't need this if the name is app.overlay
#set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_LIST_DIR}/app.overlay")
# grabs the device tree file for the board we are trying to use
set(BOARD rpi_pico/rp2040/w) 

message(STATUS "Overlay path: ${DTC_OVERLAY_FILE}")
message(STATUS "Exists: ")
if(EXISTS "${DTC_OVERLAY_FILE}")
  message(STATUS "  yes")
else()
  message(STATUS "  NO")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_starter)

target_sources(app PRIVATE src/main.cpp)