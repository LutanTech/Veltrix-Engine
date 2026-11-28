
cmake_minimum_required(VERSION 3.20)

project(Veltrix
    VERSION 0.1.0
    DESCRIPTION "Independent browser engine by Lutan Tech"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(lexbor CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

add_executable(Veltrix
    engine/main.cpp
    engine/core/engine.cpp
    engine/dom/node.cpp
    engine/dom/document.cpp
    engine/html/parser.cpp
    engine/css/style.cpp
    engine/css/parser.cpp
    engine/css/style_engine.cpp
    engine/layout/layout_engine.cpp
    engine/render/display_list.cpp
    engine/render/font_manager.cpp
    engine/render/text_renderer.cpp
    engine/render/display_list_builder.cpp
    engine/css/tokenizer.cpp
    engine/layout/text_layout.cpp
)

add_subdirectory(
    "D:/PYTHON/Veltrix Engine/freetype-2.15.0"
    freetype-build
)

target_include_directories(Veltrix PRIVATE engine)

target_link_libraries(Veltrix PRIVATE
    freetype
    SDL3::SDL3
    lexbor::lexbor
    $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>
)
