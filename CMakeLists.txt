cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(NXR VERSION 1.4.5)

if (IOS OR "${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
    message(FATAL_ERROR "NXR does not support iOS. Supported: Windows, macOS and Android 64-bit")
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

if (NOT COMMAND CPMAddPackage)
    include($ENV{GEODE_SDK}/cmake/CPM.cmake)
endif()

set(IMGUI_VERSION "v1.92.3")
CPMAddPackage("gh:matcool/gd-imgui-cocos#geode")

target_include_directories(${PROJECT_NAME} PRIVATE libs)
target_link_libraries(${PROJECT_NAME} imgui-cocos)

if (ANDROID)
    target_link_libraries(${PROJECT_NAME} mediandk)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

setup_geode_mod(${PROJECT_NAME})
