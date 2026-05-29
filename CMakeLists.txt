cmake_minimum_required(VERSION 3.16)
project(GifEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

find_library(GIF_LIB gif REQUIRED)
find_path(GIF_INCLUDE_DIR gif_lib.h REQUIRED)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/gif_reader.cpp
    src/gif_writer.cpp
    src/optimizer.cpp
    src/player_widget.cpp
    src/editor_panel.cpp
    src/range_slider.cpp
    src/cli.cpp
)

set(HEADERS
    src/mainwindow.h
    src/gif_reader.h
    src/gif_writer.h
    src/optimizer.h
    src/player_widget.h
    src/editor_panel.h
    src/range_slider.h
    src/cli.h
)

add_executable(gif-editor ${SOURCES} ${HEADERS})

set_target_properties(gif-editor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.gifeditor.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

target_include_directories(gif-editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GIF_INCLUDE_DIR}
)

target_link_libraries(gif-editor PRIVATE
    Qt6::Widgets
    ${GIF_LIB}
)

install(TARGETS gif-editor
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
