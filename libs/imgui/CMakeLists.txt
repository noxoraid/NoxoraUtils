cmake_minimum_required(VERSION 3.21)

add_library(imgui STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(imgui PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 10
    UNITY_BUILD_UNIQUE_ID "NXR_IMGUI_UNITY_ID"
)
