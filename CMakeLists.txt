cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(NXR VERSION 1.3.2)

if (NOT ANDROID AND NOT IOS AND NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
    message(FATAL_ERROR "NXR supports only Android64 and iOS")
endif()

if (ANDROID AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "NXR supports only the arm64-v8a Android ABI")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

target_include_directories(${PROJECT_NAME} PRIVATE libs)

if (ANDROID)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

setup_geode_mod(${PROJECT_NAME})
