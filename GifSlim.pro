QT       += widgets
TARGET    = gif-slim
TEMPLATE  = app
CONFIG   += c++17
VERSION   = 1.0.0

QMAKE_TARGET_DESCRIPTION = GIF Slim
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2026

win32:exists(assets/gif-slim.ico) {
    RC_ICONS = assets/gif-slim.ico
}

macx:exists(assets/gif-slim.icns) {
    ICON = assets/gif-slim.icns
}

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/gif_reader.cpp \
    src/gif_writer.cpp \
    src/optimizer.cpp \
    src/player_widget.cpp \
    src/editor_panel.cpp \
    src/range_slider.cpp \
    src/cli.cpp

HEADERS += \
    src/mainwindow.h \
    src/gif_reader.h \
    src/gif_writer.h \
    src/optimizer.h \
    src/player_widget.h \
    src/editor_panel.h \
    src/range_slider.h \
    src/cli.h

INCLUDEPATH += src /usr/include
LIBS += -lgif

target.path = /usr/bin
INSTALLS += target
