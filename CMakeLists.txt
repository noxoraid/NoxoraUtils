cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(NXR VERSION 1.1.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

target_include_directories(${PROJECT_NAME} PRIVATE libs libs/imgui)

set(HAS_IMGUI ON)
add_subdirectory(libs/imgui)
CPMAddPackage("gh:prevter/gd-imgui-cocos#6383c75")

target_link_libraries(${PROJECT_NAME} imgui-cocos imgui)

if (ANDROID AND ANDROID_ABI STREQUAL "arm64-v8a")
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

if (ANDROID AND ANDROID_ABI STREQUAL "armeabi-v7a")
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

setup_geode_mod(${PROJECT_NAME})
