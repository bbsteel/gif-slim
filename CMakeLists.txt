cmake_minimum_required(VERSION 3.16)
project(GifEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# giflib
find_library(GIF_LIB gif REQUIRED)
find_path(GIF_INCLUDE_DIR gif_lib.h REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/gif_reader.cpp
    src/gif_writer.cpp
    src/optimizer.cpp
    src/player_widget.cpp
    src/editor_panel.cpp
)

set(HEADERS
    src/mainwindow.h
    src/gif_reader.h
    src/gif_writer.h
    src/player_widget.h
    src/editor_panel.h
    src/frame_cache.h
    src/optimizer.h
)

add_executable(gif-editor ${SOURCES} ${HEADERS})

target_include_directories(gif-editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GIF_INCLUDE_DIR}
)

target_link_libraries(gif-editor PRIVATE
    Qt6::Widgets
    ${GIF_LIB}
)

# Install
install(TARGETS gif-editor DESTINATION bin)
